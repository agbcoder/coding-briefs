#!/bin/bash

# Verifies the compound MD5 used by AWS for a file, and optionally downloads
# the file.

# An S3 URI and a local file path are provided. This script:
# - Queries S3 the ETag and chunk_size of the file in the URI
# - Downloads the file if missing in the local path, otherwise uses the
#   exisiting one.
#	If no local path is provided, current dir and URI filename are assumed.
# - Computes the "compound" MD5 of the file using the chunk_size and num of parts
# - Compares the computed "compound" MD5 to retrieved ETag and returns
#   a value informing whether they match or not. 

# Arguments:
# $1 - S3 URI of the file in S3
# $2 - (optional) path to local file where file is, or has to be downloaded to


# The following variables control script behaviour but are not passed
# as parameters. You can customize behaviour modifying them here.
# **empty**


### No changes below this point ###


# Computes the Multipart MD5 of the passed filename
# $1 - name of the variable which returns loaded with the result
# $2 - path to the file
# $3 - size of each part (chunk_size in multipart jargon)
# $4 - num of parts in which the file is stored
# returns in var named $1 the MD5 hash as built by AWS for ETag
#
# While $3 and $4 are redundant, both parameteres should be queried
# by the caller, and saves an extra call within the function
function computeMultipartMD5() {
	local __retVarName=$1		# name of the var which returns with the value
	local filename=$2
	local chunkSize=$3
	local numParts=$4

	local lastPart=$(( ${numParts} - 1 ))
	local md5Result=
	local argNumChunks=

	# compute the MD5 of each part and concatenate them all as a string
	local md5Concat=
	for partNum in `seq 0 ${lastPart}`
	do		
		# last part can be bigger than the others, so don't limit read size in last
		argNumChunks="count=1"
		if [ ${partNum} -eq ${lastPart} ]; then argNumChunks=""; fi 	# (read rest of file)

		md5Result=`dd status=none bs=${chunkSize} skip=${partNum} ${argNumChunks} if=${filename} | md5sum | cut -d' ' -f1`
		echo "${partNum} ${md5Result}"

		md5Concat=${md5Concat}${md5Result}
	done

	local md5Compound=
	if [[ ${numParts} -eq 1 ]];
	then
		md5Compound=${md5Concat}
	else
		# compute the MD5 of the concatenated string; requires passing it to binary
		local md5Ofmd5Concat=`echo ${md5Concat} | xxd -r -p - | md5sum | cut -d' ' -f1`
		# add final dash and number of parts
		md5Compound=${md5Ofmd5Concat}-${numParts}
	fi

	eval $__retVarName="'${md5Compound}'"
}

# Parses the S3 URI passes, returns bucket + key, EXITS script on error
# $1 - name of the var which returns with the bucket name
# $2 - name of the var which returns with the key
# $3 - name of the var which returns with the file name
# $4 - URI to parse
function parseS3Uri() {
	local __retVarBucket=$1
	local __retVarKey=$2
	local __retVarFilename=$3
	local lS3Uri=$4

	local bucket=
	local key=
	local filename=

	# check uri starts with 's3://''
	local scheme=${lS3Uri:0:5}
	scheme=${scheme/S/s}
	if [ "${scheme}" != "s3://" ]; then
		echo "ERROR: not an S3 URI"
		exit -1
	fi

	# get the filename
	filename=`basename ${lS3Uri}`

	# remove the scheme to split bucket and key
	local work=${lS3Uri:5}
	bucket=`echo ${work} | cut -d'/' -f1`
	key=`echo ${work} | cut -d'/' -f2-`

	eval $__retVarBucket="'${bucket}'"
	eval $__retVarKey="'${key}'"
	eval $__retVarFilename="'${filename}'"
}

# Gets required metadata for S3 URI from S3 server
# $1 - name of the var which returns with the chunk_size
# $2 - name of the var which returns with the num of parts
# $3 - name of the var which returns with the ETag
# $4 - S3 bucket
# $5 - S3 object's key
# I try to avoid a JSON parser
function getMetadataOfInterest() {
	local __retVarChunkSize=$1
	local __retVarNumParts=$2
	local __retVarETag=$3
	local lS3Bucket=$4
	local lS3Key=$5

	local headResult=`aws s3api head-object --bucket ${s3Bucket} --key ${s3Key} --part 1 --query='[ContentLength, PartsCount, ETag]'`
	
	# remove decorators in response data
	headResult=`echo -n ${headResult} | tr -d "\n" | tr -d '[]\\\"[:blank:]'`

	local __chunkSize=`echo -n ${headResult} | cut -d',' -f1`
	local __ETag=`echo -n ${headResult} | cut -d',' -f3`

	local __numParts=`echo -n ${headResult} | cut -d',' -f2`
	if [ "${__numParts}" = "null" ]; then __numParts=1; fi

	eval $__retVarChunkSize="'${__chunkSize}'"
	eval $__retVarNumParts="'${__numParts}'"
	eval $__retVarETag="'${__ETag}'"
}


# Parse arguments
s3Uri=$1
localFilepath=

parseS3Uri s3Bucket s3Key s3Filename ${s3Uri}

if [ $# -gt 1 ]
then
	localFilepath=$2
else
	localFilepath=`readlink -f ./${s3Filename}`
fi

# Get metadata needed from server
getMetadataOfInterest s3ChunkSize s3NumParts s3ETag ${s3Bucket} ${s3Key}

# If local file does not exist, download it
if [ ! -f ${localFilepath} ]; then
	aws s3 cp ${s3Uri} ${localFilepath}
fi

# Compute the 'compound' MD5 of the local file
computeMultipartMD5 computedMD5 ${localFilepath} ${s3ChunkSize} ${s3NumParts}

# Compare the computed value vs ETag and return proper value
if [ "${computedMD5}" = "${s3ETag}" ]; then
	exit 0
else
	echo "Error downloading file ${s3Uri}  ComputedMD5:${computedMD5}  ETag:${s3ETag}"
	exit -1
fi

exit -2

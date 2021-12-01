python demos
============

I don't have much experience in python nor have used in really large projects, but eventually
I learned for tasks which fits it well.

### updateServers

Uses boto3 to query and command AWS; I did not have time enough to study how properly use boto3,
but well, it actually works. It receives an AWS NLB name and asks for the hosts of the target group,
unregisters the host the script is running at, runs a custom back up task, then registers the host
again in the Target Group. All, of course, requires of the proper credentials in the host.

I have good experience using AWS SDK C++ for uploading and downloading from S3, but I could not
include any demonstrator yet since the code I have programmed are important components of the
company I worked at back then.


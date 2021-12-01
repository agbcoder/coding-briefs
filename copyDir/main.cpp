/*
 * Problem to solve is:
 * Copy all the files from one directory to another, avoiding to copy the existent ones.
 * It may happen the file exists in the target dir with a different name.
 */

/* Makefile taken from a previous little demo; I borrowed some minutes to force 'obj' dir
 * creation if it does not exist before building, then add rules for C files.
 * Time used: 4h + 2.5h = 6.5h, in one day, 2 sessions.
 */

// I won't use unix 'diff' command just because I think it's not the goal of the exercise.
// It's easy to write a function to diff 2 files by comparing byte per byte... I'd prefer
// to use MD5 or some other kind of hash... actually... well, it is the way to go, since
// I need to compare EACH file in the input dir with EACH file in the output one (in the
// general case), so full file comparison is very inefficient.
// I've found a tiny and independent MD5 implementation which I can include here,
// so let's go for it.

// Error handling limited to comment + abort in most cases.

extern "C" {
#include "md5.h"
}

#include <dirent.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <unistd.h>

#include <string.h>

#include <functional>
#include <algorithm>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <stdexcept>

// Barely a data struct; I _really_ think it does not deserve a class.
struct FileEntry {
  std::string name;
  unsigned long size;
  bool isDir;

  bool md5Cached;
  unsigned char md5[16];

  FileEntry() { memset(md5, 0, 16); }
  FileEntry(const char *_name, unsigned long _size, bool _isDir) :
    name( _name ), size(_size), isDir(_isDir)
  {
    memset(md5, 0, 16);
  }

};

int filter_skip_current_and_parent(const struct dirent *entry)
{
    return ( strcmp("." , entry->d_name) &&
             strcmp("..", entry->d_name)    );
}

int filter_skip_dirs(const struct dirent *entry)
{
    return ( (entry->d_type != DT_DIR) &&      // only available in Linux & BSD
             filter_skip_current_and_parent( entry ) );
}

// Returns 0 on success, or 'scandir' error code otherwise
int scanDirEntries(std::vector<FileEntry>& fileEntries, std::string dirName, bool skipDirs)
{
    struct dirent **namelist;

    // exclude dirs if so requested; otherwise just . and ..
    int (*filter_func)(const struct dirent *) = skipDirs ? filter_skip_dirs :
                                                filter_skip_current_and_parent;

    int numEntries = scandir( dirName.c_str(), &namelist, filter_func, alphasort );
    if ( numEntries == -1 ) return errno;

    // Invoke stat for each file and load its size
    struct stat statbuf;
    for(int i=0; i<numEntries; i++) {
        //std::cout << "Reading data for file " << namelist[i]->d_name << std::endl;

        std::string fullPath = dirName + "/" + namelist[i]->d_name;
        int retStat = stat( fullPath.c_str(), &statbuf );
        if ( retStat ) {
            std::cout << "Err retrieving data for file; skipping " << fullPath << std::endl;
            continue;                   // handle as required; I just skip the file
        }

        // If this code not to be externally evaluated, I think I'd use a map indexed by
        // filename even though it is not required. It is handy, clear, and almost cheap
        // for the data volumes involved.
        fileEntries.emplace_back( namelist[i]->d_name, statbuf.st_size,
                                  namelist[i]->d_type == DT_DIR );
    }

    // Free the array allocated by system call
    for(int i=0; i<numEntries; i++) free( namelist[i] );
    free( namelist );

    return 0;
}

// Returns 0 on success, errno on error, for sure related to file missing, etc.
int computeMD5(std::string& filepath, unsigned char *md5Sum)
{
    const int bufSize = 2048;

    int fd = open( filepath.c_str(), O_RDONLY );
    if ( fd == -1 ) return errno;

    char *buf = (char *) malloc( bufSize * sizeof(char) );   // not really needed dynamic

    MD5_CTX ctx;
    MD5_Init(&ctx);

    int nbytes;
    while (1) {
        nbytes = read(fd, buf, bufSize);

        if ( nbytes == -1 ) { free( buf ); close(fd); return errno; }   // err
        if ( !nbytes ) break;   // EOF

        MD5_Update(&ctx, buf, nbytes);
    }

    MD5_Final( md5Sum, &ctx );

    printf("MD5 sum: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x - %s\n",
           md5Sum[ 0], md5Sum[ 1], md5Sum[ 2], md5Sum[ 3],
           md5Sum[ 4], md5Sum[ 5], md5Sum[ 6], md5Sum[ 7],
           md5Sum[ 8], md5Sum[ 9], md5Sum[10], md5Sum[11],
           md5Sum[12], md5Sum[13], md5Sum[14], md5Sum[15],
           filepath.c_str());

    free( buf );
    close(fd);

    return 0;
}

void copyFile(std::string& srcFilepath, std::string& dstFilepath) {

    // Taken from StackOverflow:
    // https://stackoverflow.com/questions/46535422/c-copying-another-file

    std::ifstream ssrc;
    std::ofstream sdst;

    ssrc.open(srcFilepath, std::ios::in  | std::ios::binary);
    sdst.open(dstFilepath, std::ios::out | std::ios::binary);
    sdst << ssrc.rdbuf();
}

bool copyDiffFileFromSrcDirToDstDir(std::string& src, std::string& dst)
{

    /* First approach is to begin computing MD5 of all files in both source and dest.
     * Thinking of edge cases, it actually is not very efficient in some of them;
     * let's visit a couple:
     *
     * 1) Think source dir has just 1 file and dest dir has hundreds. We may end
     *    computing MD5 for (up to) hundred of files without need.
     *
     * 2) File size is very good discriminator at a very low cost. Think of hundreds
     *    of files in both dirs and none of the same size: hundreds of MD5 computed
     *    for no use.
     *
     * So we'll go the following way when comparing files from src vs dst:
     *   - first compare the file size; if diff, files are diff
     *   - if size is equal, then compare MD5
     *   - MD5 is computed only when needed, then CACHED for next use
     */

    // Load all entries in source dir
    std::vector<FileEntry> srcEntries;
    int srcErrScan = scanDirEntries( srcEntries, src, false );
    if ( srcErrScan ) {
        if      ( srcErrScan == ENOENT  ) std::cout << "Err: source dir missing" << std::endl;
        else if ( srcErrScan == ENOTDIR ) std::cout << "Err: source dir not dir" << std::endl;
        return false;
    }

    // Load all entries in destination dir
    std::vector<FileEntry> dstEntries;
    int dstErrScan = scanDirEntries( dstEntries, dst, true );
    // create dir if missing
    if ( dstErrScan == ENOENT  ) {
        std::cout << "Destination dir missing, creating it." << std::endl;
        mkdir( dst.c_str(), 0755 );         // error handling omitted here
        dstErrScan = scanDirEntries( dstEntries, dst, true );
    }

    if ( dstErrScan ) {
        if      ( srcErrScan == ENOENT  ) std::cout << "Err: dest   dir missing" << std::endl;
        else if ( srcErrScan == ENOTDIR ) std::cout << "Err: dest   dir not dir" << std::endl;
        return false;
    }

    // The very overrated and slightly overheading lambda functions are always a nice to have
    // when demonstrating C++17...
    std::function<void(const char *, std::vector<FileEntry>& )> printEntries =
            [] (const char *memberName, std::vector<FileEntry> & entries) {
        std::cout << memberName << " dir entries:   (isDir - Size - Name)" << std::endl;

        for(FileEntry e: entries)
            std::cout << e.isDir << " - " << std::setw(11) << e.size << " - " << e.name << std::endl;

        std::cout << std::endl;
    };

    printEntries( "Source", srcEntries );
    printEntries( "Dest  ", dstEntries );

    // Iterate through source dir files
    for(FileEntry srcEntry: srcEntries) {

        // Write down the full path of the file and its potential duplicate.
        // Not very elegant here, but I use them in 3 different places...
        std::string srcFilepath = src + "/" + srcEntry.name;
        std::string dstFilepath = dst + "/" + srcEntry.name;    // just use same name than source
        // About the naming of the file in destination... I ignore edge cases, use the source one

        // If entry is a dir, call this function recursively... it does work!
        if ( srcEntry.isDir ) {
            std::cout << "Entering recursion for dir " << srcEntry.name << std::endl;
            if ( false == copyDiffFileFromSrcDirToDstDir( srcFilepath, dstFilepath ) ) return false;
            continue;
        }

        // This flow if entry not a dir (let's assume is a regular file; not considering links, etc).
        bool copyThisFile = true;

        auto currDstEntryIt = dstEntries.begin();
        // Search next entry in destination of the same size
        while ( currDstEntryIt != dstEntries.end() ) {

            currDstEntryIt = std::find_if( currDstEntryIt, dstEntries.end(),
                                           [&srcEntry](const FileEntry& entry) {
                                                return (entry.size == srcEntry.size);
                                            }   // cheeerio!, a proper use of a lambda
                                         );

            // Same size found; now check MD5 sum.
            if ( currDstEntryIt != dstEntries.end() ) {
                std::cout << srcEntry.name << " in source dir is same size than " <<
                      currDstEntryIt->name << " in dest dir. Size=" << srcEntry.size << std::endl;

                // Reduced error handling; just panic. Introducing exceptions, too.

                if ( !srcEntry.md5Cached ) {
                    if ( 0 == computeMD5( srcFilepath, srcEntry.md5 ) ) srcEntry.md5Cached = true;
                    else throw new std::runtime_error("Err computing MD5 in source");
                }

                if ( !currDstEntryIt->md5Cached ) {
                    std::string dstEntryPath = dst + "/" + currDstEntryIt->name;
                    if ( 0 == computeMD5( dstEntryPath, currDstEntryIt->md5 ) )
                        currDstEntryIt->md5Cached = true;
                    else throw new std::runtime_error("Err computing MD5 in dest");
                }

                if ( !memcmp( srcEntry.md5, currDstEntryIt->md5, 16 ) ) {
                    std::cout << "Skipping " << srcEntry.name << "; same MD5 than " <<
                                        currDstEntryIt->name << std::endl;
                    copyThisFile = false;
                    break;
                }

            } // if a dest entry of the same size found in this search

            currDstEntryIt++;

        } // gone through all dest entries

        if ( copyThisFile ) {
            // As pointed out above, I just omit any checks on target file name.
            copyFile( srcFilepath, dstFilepath );
        }

    } // next source entry

    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        std::cout << "No valid arguments, usage: " << argv[0] << " <dir_source> <dir_target>" << std::endl;
        return -1;
    }

    std::string dirIn  = argv[1];
    std::string dirOut = argv[2];
    std::cout << "Proceeding to copy different files from " << dirIn << " to " << dirOut << std::endl;

    // uhmmm... if I'd support recursion... fun...
    bool someErr = copyDiffFileFromSrcDirToDstDir(dirIn, dirOut);

    return (someErr?-2:0);
}

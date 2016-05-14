# rsfs
This repository stores the implementation of the Really Simple File System.

RSFS was proposed as an exercise for an Operating Systems class in 2012. It simulates the behavior of a simple FAT file system by running on top of a virtual disk. The only file implemented by myself (and college colleagues) is the fs.c that acts as simple FAT, the rest of the files were provided by professor Gustavo Maciel Dias Vieira. 

After building the program, run it by executing:

./rsfs <name of image> <size> 
  - <name of image>: name of the virtual image that will be created to simulate the disk.
  - <size>: size of image in MB.

After the virtual disk image is up and running, it's possible to use the following shell commands to manipulate files:

format
  - format the disk
  
list
  - list all open files

create <file>
  - create an empty file inside the virtual disk with the name <file>
  
remove <file>
  - remove an existing file with name <file>

copy <file1> <file2>
  - copy the content of <file1> into <file2>
  
copyf <realfile> <file>
  - copy the content of a <realfile> in the same folder as the disk file into a <file> existing inside the virtual disk.

copyt <file> <realfile>
  - copy the content of a <file> from the virtual disk to a <realfile> outside the disk in the same folder.

exit
  - leave RSFS program.

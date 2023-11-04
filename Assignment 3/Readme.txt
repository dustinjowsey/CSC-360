Program diskinfo is used to display basic info about a FAT12 image.

To use the program.
1. Run ./Makefile
2. Run ./diskinfo <disk_image_name>.IMA

Program disklist is used to display all files and directories on a FAT12 image.
***Can not display contents of folders that take up more than one sector (ran out of time)***

To use the program.
1. Run ./Makefile
2. Run ./disklist <disk_image_name>.IMA

Program diskget is used to copy a specified file from a FAT12 image to the current working directory
***Cannot retrieve files from folders that occupy multiple sectors***
***Cannot retrieve folders from the image***

To use the program.
1. Run ./Makefile
2. Run ./diskget <disk_image_name>.IMA <file_name_to_retrieve.ext>
***Note the file name will usually be in uppercase***

Program diskput is used to copy a file from the current working directory to a folder specified on a FAT12 image
***Again if the file is in a folder that is more than one sector there is a good chance it will not be found***

To use the program.
1. Run ./Makefile
2. Run ./diskput <disk_image_name>.IMA </SUB1/SUB2/filename.ext>
***Note that since directories are in caps on the image if you enter lowercase the program will convert it to
   uppercase.***


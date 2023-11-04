#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

void use_mmap(int fd, struct stat sb);
void search_sub_directory(int offset, int bytes_per_sector, char* p);
int file_count = 0;

int main(int argc, char *argv[]){

	if(argc > 2){
		fprintf(stdout, "Error: Too many arguments, can only process one IMA file at a time. Please run as follows:\n./diskinfo <diskname>.IMA\n");
		exit(EXIT_FAILURE);
	}else if(argc < 2){
		fprintf(stdout, "Error: Too few arguments, must provide a IMA file to process. Please run as follows:\n./diskinfo <diskname>.IMA\n");
		exit(EXIT_FAILURE);
	}

	int fd;
	struct stat sb;
	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);

	//If file size at most a 1GB use mmap
	if(sb.st_size <=  1000000000){
		use_mmap(fd, sb);
	}
}

void use_mmap(int fd, struct stat sb){
	char* p = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(p == MAP_FAILED){
		fprintf(stdout, "Error: could not map disk returned error number %d\n", errno);
		exit(EXIT_FAILURE);
	}
	int addr, info, number_of_FAT_copies, sectors_per_FAT;
	//OS Name
	//get 8 bytes starting from 4th byte to 11th byte (i.e index 3->A) at address 0x0000X
	char OS_name[9];
	for(int i = 0; i < 8; i++){
		OS_name[i] = p[3 + i];
	}
	OS_name[8] = '\0';

	//Total size of disk
	addr = *(unsigned short *)(p + 19);
	int number_of_sectors = (addr & 0xFFFF);

	addr = *(unsigned short *)(p + 11);
	int bytes_per_sector = (addr & 0xFFFF);

	//Number of FAT copies
	addr = *(unsigned short *)(p + 16);
	number_of_FAT_copies = (addr & 0xFF);

	//Secotrs per FAT
	addr = *(unsigned short *)(p + 22);
	sectors_per_FAT = (addr & 0xFFFF);

	//Sectors per cluster
	addr = *(unsigned short *)(p + 13);
	int sectors_per_cluster = (addr & 0xFF);

	//Label of disk
	char disk_label[9];
	//Loop through directory entries until find attribute 0x08 (Find volume label)
	int is_directory;
	addr = *(unsigned short*)(p + 17);
	int offset = number_of_FAT_copies*sectors_per_FAT*bytes_per_sector + bytes_per_sector;
	int max_directory_entries = (addr & 0xFFFF);
	int char_check;
	for(int i = 0; i < max_directory_entries; i++){
		addr = *(unsigned short *)(p + offset + (i*32) + 11); //32 bytes per entry
		//check if file is long file name
		info = (addr & 0xFF);
		if(info == 0x0F){
			continue;
		}else{
			info = (addr & 0x08);
		}
		//Check if file is volume label file
		if(info == 8){
			for(int j = 0; j <= 8; j++){
				addr = *(unsigned short*)(p + offset + (i*32) + j);
				char_check = (addr & 0xFF);
				if(char_check == 32 || j == 8){
					disk_label[j] = '\0';
					break;
				}
				disk_label[j] = p[offset + (i*32) + j];
			}
		}
	}
	//For file count
	int first_logical_cluster, FLSaddr;
	int directory_offset;//What sector is the folder in the data section
	for(int i = 0; i < max_directory_entries; i++){
		FLSaddr = *(unsigned short *)(p + offset + (i*32) + 26);
		first_logical_cluster = (FLSaddr & 0xFFFF);

		//if first logical sector is 1 or 0 then don't bother looking at the file
		if(first_logical_cluster == 0 || first_logical_cluster == 1){
			continue;
		}
		addr = *(unsigned short *)(p + offset + (i*32) + 11);
		info = (addr & 0xFF);
		is_directory = (addr & 0x10);
		//if file is not a directory
		if(is_directory == 0){
			file_count++;
		}
		//If file is a directory need to loop through all directories entries
		else{
			directory_offset = (31 + first_logical_cluster)*bytes_per_sector;
			search_sub_directory(directory_offset, bytes_per_sector, p);
		}

	}

	//Free disk space
	int position = 4 + bytes_per_sector;
	int temp = 0;
	int unused_sectors = 0;
	//want to check 2880 (number of sectors - 33 (starts at sector 0 so subtract extra sector))
	//i represents each cluster where i=0 points to sector 32
	for(int i = 0; i < number_of_sectors - 33;){ //Look at two clusters every iteration
		addr = *(unsigned short*)(p + position);
		info = (addr & 0xFF);
		position += 1;
		addr = *(unsigned short*)(p + position);
		temp = (addr & 0xF) << 8;
		info = (info | temp);
		i++;

		if(info == 0){
			unused_sectors+=sectors_per_cluster;
		}

		//Need to check number of sectors looked at incase we are checking an odd number of sectors
		if(i >= (number_of_sectors -33)){
			break;
		}

		info = (addr & 0xF0) >> 4;
		position += 1;
		addr = *(unsigned short*)(p + position);
		temp = (addr & 0xFF) << 4;
		info = (info | temp);
		position += 1;
		i++;
		if(info == 0){
			unused_sectors+=sectors_per_cluster;
		}
	}

	fprintf(stdout, "OS Name: %s\n", OS_name);
	fprintf(stdout, "Label of the disk: %s\n", disk_label);
	fprintf(stdout, "Total size of the disk: %d bytes\n", number_of_sectors * bytes_per_sector);
	fprintf(stdout, "Free size of the disk: %d bytes\n", (unused_sectors * bytes_per_sector));
	fprintf(stdout, "\n==============\nNumber of files in the disk: %d\n\n==============\n", file_count);
	fprintf(stdout, "Number of FAT copies: %d\n", number_of_FAT_copies);
	fprintf(stdout, "Sectors per FAT: %d\n", sectors_per_FAT);

	int ret_val = munmap(p, (number_of_sectors*bytes_per_sector));
	if(ret_val != 0){
		fprintf(stdout, "Error: Unmapping memory fialed\n");
		exit(EXIT_FAILURE);
	}
}

void search_sub_directory(int offset, int bytes_per_sector, char* p){
	int is_directory, addr, first_logical_cluster;
	for(int i = 2; i < 16; i++){//check all sub-directory entries
		addr = *(unsigned short *)(p + (i*32) + offset + 11); //32 bytes per entry
		is_directory = (addr & 0x10);
		addr = *(unsigned short *)(p + (i*32) + offset + 26);
		first_logical_cluster = (addr & 0xFFFF);
		if(first_logical_cluster == 1 || first_logical_cluster == 0){
			continue;
		}
		if(is_directory == 0){
			file_count++;
		}else{
			offset = (31 + first_logical_cluster)*bytes_per_sector;
			search_sub_directory(offset, bytes_per_sector, p);
		}
	}
	return;

}

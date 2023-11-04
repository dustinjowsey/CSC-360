#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

void use_mmap(int fd, struct stat sb, char* file);
void search_sub_directory(int offset, int folder_sector ,int bytes_per_sector, char* p, char* file);
void copy_file_to_dir(char* filename, int first_logical_cluster, int bytes_per_sector, char* p);

int main(int argc, char *argv[]){

	if(argc > 3){
		fprintf(stdout, "Error: Too many arguments, can only process one IMA file at a time. Please run as follows:\n./diskinfo <diskname>.IMA <file.ext>\n");
		exit(EXIT_FAILURE);
	}else if(argc < 3){
		fprintf(stdout, "Error: Too few arguments, must provide a IMA file to process. Please run as follows:\n./diskinfo <diskname>.IMA <file.ext>\n");
		exit(EXIT_FAILURE);
	}

	int fd;
	struct stat sb;
	fd = open(argv[1], O_RDWR);
	fstat(fd, &sb);

	//If file size at most a 1GB use mmap
	if(sb.st_size <=  1000000000){
		use_mmap(fd, sb, argv[2]);
	}
	return 0;
}

void use_mmap(int fd, struct stat sb, char* file){
	char* p = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(p == MAP_FAILED){
		fprintf(stdout, "Error: could not map disk returned error number %d\n", errno);
		exit(EXIT_FAILURE);
	}
	int addr, number_of_FAT_copies, sectors_per_FAT;

	//Total size of disk
	addr = *(unsigned short *)(p + 11);
	int bytes_per_sector = (addr & 0xFFFF);

	//Number of FAT copies
	addr = *(unsigned short *)(p + 16);
	number_of_FAT_copies = (addr & 0xFF);

	//Secotrs per FAT
	addr = *(unsigned short *)(p + 22);
	sectors_per_FAT = (addr & 0xFFFF);

	int is_directory;
	addr = *(unsigned short*)(p + 17);
	int offset = number_of_FAT_copies*sectors_per_FAT*bytes_per_sector + bytes_per_sector;
	int max_directory_entries = (addr & 0xFFFF);

	//Scan all files on machine
	int first_logical_cluster, FLSaddr;
	int directory_offset;//What sector is the folder in, in the data section
	char filename[14];
	fprintf(stdout, "File: %s\n", file);
	for(int i = 0; i < max_directory_entries; i++){
		FLSaddr = *(unsigned short *)(p + offset + (i*32) + 26);
		first_logical_cluster = (FLSaddr & 0xFFFF);

		//if first logical sector is 1 or 0 then don't bother looking at the file
		if(first_logical_cluster == 0 || first_logical_cluster == 1){
			continue;
		}
		addr = *(unsigned short *)(p + offset + (i*32) + 11);
		is_directory = (addr & 0x10);
		//If a file then check file name to see if it is the file we are looking for
		if(is_directory == 0){
			int char_check;
			for(int j = 0; j <= 8; j++){
				addr = *(unsigned short*)(p + offset + (i*32) + j);
				char_check = (addr & 0xFF);
				if(char_check == 32 || j == 8){
					filename[j] = '.';
					j++;
					int count = 0;
					while(1 == 1){
						addr = *(unsigned short*)(p + offset + (i*32) + 8 + count);
						char_check = (addr & 0xFF);
						if(char_check == 0 || char_check == 32 || count == 3){
							filename[j + count] = '\0';
							break;
						}else{
							filename[j + count] = p[offset + (i*32) + 8 + count];
						}
						count++;
					}
					break;
				}
				filename[j] = p[offset + (i*32) + j];
			}
			if(strcmp(filename, file) == 0){
				fprintf(stdout,"found file!\n");
				copy_file_to_dir(filename, first_logical_cluster, bytes_per_sector, p);
			}
		}
		//If file is a directory need to loop through all directories entries
		else{
			directory_offset = (31 + first_logical_cluster)*bytes_per_sector;
			search_sub_directory(directory_offset, first_logical_cluster ,bytes_per_sector, p, file);
		}

	}
	fprintf(stdout,"File not found\n");
	int ret_val = munmap(p, sb.st_size);
	if(ret_val != 0){
		fprintf(stdout, "Error: Unmapping memory failed.\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

void search_sub_directory(int offset,int folder_sector, int bytes_per_sector, char* p, char* file){
	int is_directory, addr, first_logical_cluster;
	int dir_offset;
	char filename[14];
	for(int i = 2; i < 16; i++){//check all directory entries
		addr = *(unsigned short *)(p + (i*32) + offset + 11); //32 bytes per entry
		is_directory = (addr & 0x10);
		addr = *(unsigned short *)(p + (i*32) + offset + 26);
		first_logical_cluster = (addr & 0xFFFF);
		if(first_logical_cluster == 1 || first_logical_cluster == 0){
			continue;
		}
		//If a file then check file name to see if it is the file we are looking for
		if(is_directory == 0){
			int char_check;
			for(int j = 0; j <= 8; j++){
				addr = *(unsigned short*)(p + offset + (i*32) + j);
				char_check = (addr & 0xFF);
				if(char_check == 32 || j == 8){
					filename[j] = '.';
					j++;
					int count = 0;
					while(1 == 1){
						addr = *(unsigned short*)(p + offset + (i*32) + 8 + count);
						char_check = (addr & 0xFF);
						if(char_check == 0 || char_check == 32 || count == 3){
							filename[j + count] = '\0';
							break;
						}else{
							filename[j + count] = p[offset + (i*32) + 8 + count];
						}
						count++;
					}
					break;
				}
				filename[j] = p[offset + (i*32) + j];
			}
			if(strncmp(filename, file, strlen(file) + 1) == 0){
				fprintf(stdout,"found file!\n");
				copy_file_to_dir(filename, first_logical_cluster, bytes_per_sector, p);
			}
		}else{
			dir_offset = (31 + first_logical_cluster)*bytes_per_sector;
			search_sub_directory(dir_offset,first_logical_cluster, bytes_per_sector, p, file);
		}
	}
	return;
}

void copy_file_to_dir(char* filename, int first_logical_cluster, int bytes_per_sector, char* p){
	FILE *fp;
	fp = fopen(filename, "w");
	if(fp == NULL){
		fprintf(stdout, "Error: could not open file or create file\n");
		exit(EXIT_FAILURE);
	}
	int next_entry = first_logical_cluster;
	fprintf(stdout, "FLC: %d\n", next_entry);
	int info, addr, addr1, temp, offset;
	char data[bytes_per_sector];
	fprintf(stdout, "Attempting to copy file data...\n");
	while(next_entry < 0xFF0 && next_entry != 0){
		//retrieve data
		offset = (31 + next_entry) * bytes_per_sector;
		for(int i = 0; i < bytes_per_sector; i++){
			data[i] = p[offset + i];
		}
		fprintf(fp, "%s", data);
		//get next sector
		addr = *(unsigned short*)(p + bytes_per_sector + (3 * next_entry/2));
		addr1 = *(unsigned short*)(p + bytes_per_sector + 1 + (3 * next_entry/2));
		if((next_entry % 2) == 0){
			info = (addr & 0xFF);
			temp = (addr1 & 0x0F) << 8;
			info = (info | temp);
		}else{
			info = (addr & 0xF0) >> 4;
			temp = (addr1 & 0xFF) << 4;
			info = (info | temp);
		}
		next_entry = info;
		fprintf(stdout, "next_entry: %d\n", next_entry);
	}
	fprintf(stdout, "Successes!\n");
	fclose(fp);
	exit(EXIT_SUCCESS);
}

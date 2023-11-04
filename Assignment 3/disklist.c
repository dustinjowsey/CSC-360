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

void use_mmap(int fd, struct stat sb);
void search_sub_directory(int offset, int folder_sector ,int bytes_per_sector, char* p, char* dir_name, char* folder_name);

int main(int argc, char *argv[]){

	if(argc > 2){
		fprintf(stdout, "Error: Too many arguments. Please run as follows:\n./disklist <diskname>.IMA <filename>\n");
		exit(EXIT_FAILURE);
	}else if(argc < 2){
		fprintf(stdout, "Error: Too few arguments. Please run as follows:\n./disklist <diskname>.IMA <filename>\n");
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
	return 0;

}

void use_mmap(int fd, struct stat sb){
	char* p = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if(p == MAP_FAILED){
		fprintf(stdout, "Error: could not map disk returned error number %d\n", errno);
		exit(EXIT_FAILURE);
	}
	int addr, info, number_of_FAT_copies, sectors_per_FAT;

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
	int cre_date, cre_time, mod_time, mod_date;
	int file_size;
	int hours, minutes, day, month, year;
	int search_for_directory = 0;//flag to tell us we printed everything in the directory and can go into another directory to print the contents
	fprintf(stdout, "Root\n==================\n");
	for(int i = 0; i <= max_directory_entries; i++){
		if(i == max_directory_entries && search_for_directory == 0){
			i = 0;
			search_for_directory = 1;
		}else if(i == max_directory_entries && search_for_directory == 1){
			break;
		}

		FLSaddr = *(unsigned short *)(p + offset + (i*32) + 26);
		first_logical_cluster = (FLSaddr & 0xFFFF);

		//if first logical sector is 1 or 0 then don't bother looking at the file
		if(first_logical_cluster == 0 || first_logical_cluster == 1){
			continue;
		}

		addr = *(unsigned short *)(p + offset + (i*32) + 11);
		info = (addr & 0xFF);
		if(info == 0x0F || (addr & 0x08) == 8){
			continue;
		}
		is_directory = (addr & 0x10);

		addr = *(unsigned short *)(p + offset + (i*32) + 28);
		file_size = (addr & 0xFFFFFFFF);

		cre_time = *(unsigned short *)(p + offset + (i*32) + 14);
		cre_date = *(unsigned short *)(p + offset + (i*32) + 16);
		//Date and time code taken from sample code
		year = ((cre_date & 0xFE00) >> 9) + 1980;
		month = (cre_date & 0x1E0) >> 5;
		day = (cre_date & 0x1F);
		hours = (cre_time & 0xF800) >> 11;
		minutes = (cre_time & 0x7E0) >> 5;

		int char_check;
		for(int j = 0; j <= 8; j++){
			addr = *(unsigned short*)(p + offset + (i*32) + j);
			char_check = (addr & 0xFF);
			if(char_check == 32 || j == 8){//If rest of name is empty space, don't copy it
				if(is_directory != 0){
					filename[j] = '\0';
					break;
				}
				filename[j] = '.';
				j++;
				int count = 0;
				while(1 == 1){
					addr = *(unsigned short*)(p + offset + (i*32) + 8 + count);
					char_check = (addr & 0xFF);
					if(char_check == 0 || char_check == 32){
						filename[j + count] = '\0';
					}else{
						filename[j + count] = p[offset + (i*32) + 8 + count];
					}
					if(count >= 3){
						break;
					}
					count++;
		 		}
				break;
			}
			filename[j] = p[offset + (i*32) + j];
		}

		if(is_directory == 0 && search_for_directory == 0){
			fprintf(stdout, "F %10d %20s %d-%02d-%02d %02d:%02d\n",file_size  ,filename, year, month, day, hours, minutes);
		}
		//If file is a directory need to loop through all directories entries
		else if(is_directory != 0 && search_for_directory == 0){
			mod_time = *(unsigned short *)(p + offset + (i*32) + 22);
			mod_date = *(unsigned short *)(p + offset + (i*32) + 24);
			//Date and time code taken from sample code
			year = ((mod_date & 0xFE00) >> 9) + 1980;
			month = (mod_date & 0x1E0) >> 5;
			day = (mod_date & 0x1F);
			hours = (mod_time & 0xF800) >> 11;
			minutes = (mod_time & 0x7E0) >> 5;
			fprintf(stdout, "D %10d %20s %d-%02d-%02d %02d:%02d\n",file_size  ,filename, year, month, day, hours, minutes);
		}

		else if(search_for_directory == 1 && is_directory != 0){
			directory_offset = (31 + first_logical_cluster)*bytes_per_sector;
			char buffer[20] = "";
			search_sub_directory(directory_offset, first_logical_cluster ,bytes_per_sector, p, buffer, filename);
		}

	}
	int ret_val = munmap(p, sb.st_size);
	if(ret_val != 0){
		fprintf(stdout, "Error: Unmapping memory failed.\n");
		exit(EXIT_FAILURE);
	}
}

void search_sub_directory(int offset,int folder_sector, int bytes_per_sector, char* p, char* prev_dir_name, char* folder_name){
	int is_directory, addr, first_logical_cluster, file_size;
	int cre_date, cre_time, mod_time, mod_date;
	int year, month, day, hours, minutes;
	int dir_offset;
	char filename[14];
	int search_for_directory = 0;
	char dir_name[250] = "";
	strcat(dir_name, prev_dir_name);
	strcat(dir_name, "/");
	fprintf(stdout, "%s%s\n==================\n", dir_name, folder_name);
	for(int i = 2; i <= 16; i++){//check all directory entries
		if(i == 16 && search_for_directory == 0){
			i = 2;
			search_for_directory = 1;
		}else if(i == 16 && search_for_directory == 1){
			break;
		}

		addr = *(unsigned short *)(p + (i*32) + offset + 11); //32 bytes per entry
		is_directory = (addr & 0x10);
		addr = *(unsigned short *)(p + (i*32) + offset + 26);
		first_logical_cluster = (addr & 0xFFFF);
		if(first_logical_cluster == 1 || first_logical_cluster == 0){
			continue;
		}

	//Don't have time to add a check for folders that need more than one sector
	//Would check that the entry in the FAT table for the folder and check if the next entry is 0xFFF or not.
	//Then call this function again.
	//WOuld need a flag so that we don't update the folder we are in since we are still in the same folder

		int char_check;
		for(int j = 0; j <= 8; j++){
			addr = *(unsigned short*)(p + offset + (i*32) + j);
			char_check = (addr & 0xFF);
			if(char_check == 32 || j == 8){//If rest of name is empty space, don't copy it
				if(is_directory != 0){
					filename[j] = '\0';
					break;
				}
				filename[j] = '.';
				j++;
				int count = 0;
				while(1 == 1){
					addr = *(unsigned short*)(p + offset + (i*32) + 8 + count);
					char_check = (addr & 0xFF);
					if(char_check == 0 || char_check == 32){
						filename[j + count] = '\0';
					}else{
						filename[j + count] = p[offset + (i*32) + 8 + count];
					}
					if(count >= 3){
						break;
					}
					count++;
	 			}
				break;
			}
			filename[j] = p[offset + (i*32) + j];
		}

		addr = *(unsigned short *)(p + (i*32) + offset + 28);
		file_size = (addr & 0xFFFFFFFF);

		cre_time = *(unsigned short *)(p + offset + (i*32) + 14);
		cre_date = *(unsigned short *)(p + offset + (i*32) + 16);
		//Date and time code taken from sample code
		year = ((cre_date & 0xFE00) >> 9) + 1980;
		month = (cre_date & 0x1E0) >> 5;
		day = (cre_date & 0x1F);
		hours = (cre_time & 0xF800) >> 11;
		minutes = (cre_time & 0x7E0) >> 5;

		if(is_directory == 0 && search_for_directory == 0){
			fprintf(stdout, "F %10d %20s %d-%02d-%02d %02d:%02d\n",file_size  ,filename, year, month, day, hours, minutes);
		}else if(is_directory != 0 && search_for_directory == 0){
			mod_time = *(unsigned short *)(p + offset + (i*32) + 22);
			mod_date = *(unsigned short *)(p + offset + (i*32) + 24);
			//Date and time code taken from sample code
			year = ((mod_date & 0xFE00) >> 9) + 1980;
			month = (mod_date & 0x1E0) >> 5;
			day = (mod_date & 0x1F);
			hours = (mod_time & 0xF800) >> 11;
			minutes = (mod_time & 0x7E0) >> 5;
			fprintf(stdout, "D %10d %20s %d-%02d-%02d %02d:%02d\n",file_size  ,filename, year, month, day, hours, minutes);
		}
		else if(is_directory != 0 && search_for_directory == 1){
			dir_offset = (31 + first_logical_cluster)*bytes_per_sector;
			char new_dir_name[250];
			strcat(new_dir_name, dir_name);
			strcat(new_dir_name, folder_name);
			search_sub_directory(dir_offset,first_logical_cluster, bytes_per_sector, p, new_dir_name, filename);
			for(int i = 0; i < 250; i++){
				new_dir_name[i] = '\0';
			}
		}
	}
	return;
}

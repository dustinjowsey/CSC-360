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
#include <math.h>
#include <ctype.h>
#include <time.h>

void use_mmap(int fd, struct stat sb, char* file);
void search_sub_directory(int offset, int folder_sector ,int bytes_per_sector, char* p, char* file, int count ,int num_subdirectories, int first_file_cluster, int date, int date_acs, int time);
void copy_file_to_dir(char* filename, int first_logical_cluster, int bytes_per_sector, char* p);
int add_to_FAT(char* p, int num_entries, int bytes_per_sector, int sectors_per_FAT, int number_of_FAT_copies, int sect_left, int i, int input_file_count);

char* sub_directories[250];
char* imptr;
struct stat imsb;
int fimptr;
int sub_directory_count;
int max_directory_entries;

int main(int argc, char *argv[]){

	if(argc > 3){
		fprintf(stdout, "Error: Too many arguments, can only process one IMA file at a time. Please run as follows:\n./diskinfo <diskname>.IMA <<path>/file.ext>\n");
		exit(EXIT_FAILURE);
	}else if(argc < 3){
		fprintf(stdout, "Error: Too few arguments, must provide a IMA file to process. Please run as follows:\n./diskinfo <diskname>.IMA <<path>/file.ext>\n");
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
	char* p = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	sub_directory_count = 0;
	if(p == MAP_FAILED){
		fprintf(stdout, "Error: could not map disk returned error number %d\n", errno);
		exit(EXIT_FAILURE);
	}
	int addr, info, number_of_FAT_copies, sectors_per_FAT;
	fprintf(stdout, "\nBeginning to copy file data!\n");

	char* tok = strtok(file, "/");
	sub_directories[sub_directory_count] = (char*)malloc(sizeof(char)*8);
	sub_directories[sub_directory_count] = tok;
	while(tok != NULL){
		sub_directory_count++;
		sub_directories[sub_directory_count] = (char*)malloc(sizeof(char)*8);
		tok = strtok(NULL, "/");
		sub_directories[sub_directory_count] = tok;
	}
	sub_directory_count--;

	//Total size of disk
	addr = *(unsigned short *)(p + 19);
	int number_of_sectors = (addr & 0xFFFF);

	addr = *(unsigned short *)(p + 11);
	int bytes_per_sector = (addr & 0xFFFF);

	//Get file size
	fimptr = open(sub_directories[sub_directory_count], O_RDWR);
	fstat(fimptr, &imsb);
	imptr = mmap(NULL, imsb.st_size, PROT_READ, MAP_SHARED, fimptr, 0);
	float sect_needed = (double)imsb.st_size/(double)bytes_per_sector;
	double entries_needed = ceil(sect_needed);

	//Convert all directories and the file input to uppercase
	for(int i = 0; i <= sub_directory_count; i++){
		for(int j = 0; j < strlen(sub_directories[i]); j++){
			sub_directories[i][j] = toupper(sub_directories[i][j]);
		}
	}

	//Get last write time of file
	struct tm *lst_wrt_time = (struct tm*)malloc(sizeof(struct tm));
	struct tm *lst_acs_time = (struct tm*)malloc(sizeof(struct tm));
	lst_wrt_time = localtime(&(imsb.st_mtim.tv_sec));
	lst_acs_time = localtime(&(imsb.st_atim.tv_sec));
	//Have to add 80 to stay consistent with diskget. tm measures from 1900 not 1980.
	int year = (lst_wrt_time->tm_year - 80) << 9;
	//fprintf(stdout, "year: %d\n", lst_wrt_time->tm_year);
	int month = (lst_wrt_time->tm_mon) << 5;
	int day = (lst_wrt_time->tm_mday);
	int date = year+month+day;

	int hours = (lst_wrt_time->tm_hour) << 11;
	int minutes = (lst_wrt_time->tm_min) << 5;
	int time = hours + minutes;
	//Last access date
	year = (lst_acs_time->tm_year - 80) << 9;
	month = (lst_acs_time->tm_mon) << 5;
	day = (lst_acs_time->tm_mday);
	int date_acs = year+month+day;

	//Number of FAT copies
	addr = *(unsigned short *)(p + 16);
	number_of_FAT_copies = (addr & 0xFF);

	//Secotrs per FAT
	addr = *(unsigned short *)(p + 22);
	sectors_per_FAT = (addr & 0xFFFF);

	//Sectors per cluster
	addr = *(unsigned short *)(p + 13);
	int sectors_per_cluster = (addr & 0xFF);

	addr = *(unsigned short*)(p + 17);
	int offset = number_of_FAT_copies*sectors_per_FAT*bytes_per_sector + bytes_per_sector;
	max_directory_entries = (addr & 0xFFFF);

	fprintf(stdout, "Checking available space...\n");
	//Want to count free space before modifying Image
	int position = 4 + bytes_per_sector;
	int temp = 0;
	int unused_sectors = 0;
	for(int i = 0; i < number_of_sectors - 33;){
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
		if(i >= (number_of_sectors - 33)){
			break;
		}

		info = (addr & 0xF0);
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
	if(entries_needed > unused_sectors){
		fprintf(stdout, "Error: not enough free space need %1f bytes, but only have %d bytes\n", (entries_needed * bytes_per_sector), (unused_sectors * bytes_per_sector));
	}

	fprintf(stdout, "Success! There is enough space on the image.\nCopying data to Data section and FAT's...\n");
	//Copy file to Image
	int num_entries = sectors_per_FAT*bytes_per_sector/1.5;
	int first_file_cluster;
	int sect_left = entries_needed;
	first_file_cluster = add_to_FAT(p, num_entries, bytes_per_sector, sectors_per_FAT, number_of_FAT_copies, sect_left, 0, 0);
	fprintf(stdout, "File data successfully copied to data section and FAT's\n");

	//copy file into a directory on the image
	int is_directory;
	int dir_offset;
	int file_offset;
	int count = 0;
	int temp_offset;
	//If in directory folder, find free space
	if(count == sub_directory_count){
		fprintf(stdout, "Copying file details to root directory...\n");
		for(int i = 0; i < max_directory_entries; i++){
			addr = *(unsigned short *)(p + (i*32) + offset);
			info = (addr & 0xFF);
			//Entry free
			if(info == 0x00){
				file_offset = ((i*32) + offset);
				//enter name into entry
				for(int j = 0; j < 8; j++){
					if(sub_directories[sub_directory_count][j] == '.'){
						for(int k = j; k < 8; k++){
							p[file_offset + k] = ' ';
						}
						temp_offset = j+1;
						break;
					}
					p[file_offset + j] = sub_directories[sub_directory_count][j];
				}
				file_offset += 8;
				for(int j = temp_offset; j < temp_offset + 3; j++){
					if(sub_directories[sub_directory_count][j] == '\0'){
						for(int k = j; k < 3; k++){
							p[file_offset + k] = ' ';
						}
					}
					p[file_offset + (j - temp_offset)] = sub_directories[sub_directory_count][j];
				}
				file_offset += 3;
				p[file_offset] = 0;
				file_offset += 3;
				//time entry
				p[file_offset] = (0x00FF & time);
				file_offset += 1;
				p[file_offset] = (0xFF00 & time) >> 8;
				file_offset += 1;
				p[file_offset] = (0x00FF & date);
				file_offset += 1;
				p[file_offset] = (0xFF00 & date) >> 8;
				file_offset += 1;
				p[file_offset] = (0x00FF & date_acs);
				file_offset += 1;
				p[file_offset] = (0xFF00 & date_acs) >> 8;
				file_offset += 3;
				p[file_offset] = (0x00FF & time);
				file_offset += 1;
				p[file_offset] = (0xFF00 & time) >> 8;
				file_offset += 1;
				p[file_offset] = (0x00FF & date);
				file_offset += 1;
				p[file_offset] = (0xFF00 & date) >> 8;
				file_offset += 1;
				//FLC
				p[file_offset] = (0x00FF & first_file_cluster);
				file_offset += 1;
				p[file_offset] = (0xFF00 & first_file_cluster) >> 8;
				file_offset += 1;
				//file size
				p[file_offset] = (0x00FF & imsb.st_size);
				file_offset += 1;
				p[file_offset] = (0xFF00 & imsb.st_size) >> 8;
				fprintf(stdout, "Success!\nFile has been fully copied to image.\n");
				return;
			}
		}
	}
	char dir_name[9];
	int first_logical_cluster;
	//Look for first sibdirectory to go into
	fprintf(stdout, "Searching for correct directory to copy file to...\n");
	for(int i = 0; i < max_directory_entries; i++){//check all directory entries
		addr = *(unsigned short *)(p + (i*32) + offset + 11); //32 bytes per entry
		is_directory = (addr & 0x10);
		
		addr = *(unsigned short *)(p + (i*32) + offset + 26);
		first_logical_cluster = (addr & 0xFFFF);
		if(first_logical_cluster == 1 || first_logical_cluster == 0){
			continue;
		}
		if(is_directory != 0){
			int char_check;
			for(int j = 0; j < 8; j++){
				char_check = p[offset + (i*32) + j];
				if(char_check == 0 || char_check == 32){
					dir_name[j] = '\0';
					break;
				}
				dir_name[j] = p[offset + (i*32) + j];
			}
			if(strncmp(dir_name, sub_directories[count], strlen(dir_name) + 1) == 0){
				dir_offset = (31 + first_logical_cluster)*bytes_per_sector;
				count++;
				search_sub_directory(dir_offset, first_logical_cluster, bytes_per_sector, p, sub_directories[count], count, sub_directory_count, first_file_cluster, date, date_acs, time);
				return;
			}
		}else{
			continue;
		}
	}
	fprintf(stdout, "Error: Directory not found!\n");
	int ret_val = munmap(p, sb.st_size);
	if(ret_val != 0){
		fprintf(stdout, "Error: Unmapping memory failed.\n");
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}
int add_to_FAT(char* p, int num_entries, int bytes_per_sector, int sectors_per_FAT, int number_of_FAT_copies, int sect_left, int i, int input_file_count){
	int addr, addr1, temp, temp1, next_entry, info;
	addr = *(unsigned short*)(p + bytes_per_sector + (3 * i/2));
	addr1 = *(unsigned short*)(p + bytes_per_sector + 1 + (3 * i/2));
	if((i % 2) == 0){
		info = (addr & 0xFF);
		temp = (addr1 & 0xF) << 8;
		info = (info | temp);
	}else{
		info = (addr & 0xF0) >> 4;
		temp = (addr1 & 0xFF) << 4;
		info = (info | temp);
	}
	//If last cluster in FAT mark 0xFFF
	if(sect_left == 0 && info == 0){
		if((i % 2) == 0){
			for(int k = 0; k < number_of_FAT_copies; k++){
				p[bytes_per_sector + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = 0xFF;
				temp = (0xF0 & p[bytes_per_sector + 1 + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)]);
				temp = temp | 0x0F;
				p[bytes_per_sector + 1 + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = temp;
			}
		}else{
			for(int k = 0; k < number_of_FAT_copies; k++){
				temp = (0x0F & p[bytes_per_sector + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)]);
				temp = temp | 0xF0;
				p[bytes_per_sector + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = temp;
				p[bytes_per_sector + 1 + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = 0xFF;
			}
		}
		return i;
	}

	//If not ast cluster add entries into FAT's
	if(info == 0 && sect_left > 0){
		for(int j = 0; j < bytes_per_sector; j++){
			addr = *(unsigned short*)(imptr + (input_file_count * bytes_per_sector) + j);
			temp1 = (0xFF & addr);
			p[((31 + i)*bytes_per_sector) + j] = temp1;
		}
		input_file_count++;
		sect_left--;
		int next_index = i;
		next_index++;
		next_entry = add_to_FAT(p, num_entries, bytes_per_sector, sectors_per_FAT, number_of_FAT_copies, sect_left, next_index, input_file_count);
		if((i % 2) == 0){
			for(int k = 0; k < number_of_FAT_copies; k++){
				temp = (0x00FF & next_entry);
				temp1 = (next_entry & 0x0F00) >> 8;
				p[bytes_per_sector + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = temp;
				temp = (0xF0 & p[bytes_per_sector + 1 + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)]);
				p[bytes_per_sector + 1 + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = (temp | temp1);
			}
		}else{
			for(int k = 0; k < number_of_FAT_copies; k++){
				temp = (0x0FF0 & next_entry) >> 4;
				temp1 = (0x000F & next_entry) << 4;
				p[bytes_per_sector + 1 + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = temp;
				temp = (0x0F & p[bytes_per_sector + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)]);
				p[bytes_per_sector + (3 * i/2) + (k*sectors_per_FAT*bytes_per_sector)] = (temp | temp1);
			}
		}
		return i;
	}else{
		i++;
		return add_to_FAT(p, num_entries, bytes_per_sector, sectors_per_FAT, number_of_FAT_copies, sect_left, i, input_file_count);
	}
}
void search_sub_directory(int offset, int folder_sector, int bytes_per_sector, char* p, char* file, int count, int num_subdirectories, int first_file_cluster, int date, int date_acs, int time){
	int is_directory, addr, first_logical_cluster;
	int dir_offset, info;
	int file_offset;
	char filename[14];
	int temp_offset;
	//If in correct folder, find free space
	if(count == num_subdirectories){
		for(int i = 0; i < max_directory_entries; i++){
			addr = *(unsigned short *)(p + (i*32) + offset);
			info = (addr & 0xFF);
			//Entry free
			if(info == 0x00){
				file_offset = ((i*32) + offset);
				//enter name into entry
				for(int j = 0; j < 8; j++){
					if(sub_directories[count][j] == '.'){
						for(int k = j; k < 8; k++){
							p[file_offset + k] = ' ';
						}
						temp_offset = j+1;
						break;
					}
					p[file_offset + j] = sub_directories[count][j];
				}
				file_offset += 8;
				for(int j = temp_offset; j < temp_offset + 3; j++){
					if(sub_directories[sub_directory_count][j] == '\0'){
						for(int k = j; k < 3; k++){
							p[file_offset + k] = ' ';
						}
					}
					p[file_offset + (j - temp_offset)] = sub_directories[count][j];
				}
				file_offset += 3;
				p[file_offset] = 0;
				file_offset += 3;
				//time
				p[file_offset] = (0x00FF & time);
				file_offset += 1;
				p[file_offset] = (0xFF00 & time) >> 8;
				file_offset += 1;
				p[file_offset] = (0x00FF & date);
				file_offset += 1;
				p[file_offset] = (0xFF00 & date) >> 8;
				file_offset += 1;
				p[file_offset] = (0x00FF & date_acs);
				file_offset += 1;
				p[file_offset] = (0xFF00 & date_acs) >> 8;
				file_offset += 3;
				p[file_offset] = (0x00FF & time);
				file_offset += 1;
				p[file_offset] = (0xFF00 & time) >> 8;
				file_offset += 1;
				p[file_offset] = (0x00FF & date);
				file_offset += 1;
				p[file_offset] = (0xFF00 & date) >> 8;
				file_offset += 1;
				//FLC
				p[file_offset] = (0x00FF & first_file_cluster);
				file_offset += 1;
				p[file_offset] = (0xFF00 & first_file_cluster) >> 8;
				file_offset += 1;
				//file size
				p[file_offset] = (0x00FF & imsb.st_size);
				file_offset += 1;
				p[file_offset] = (0xFF00 & imsb.st_size) >> 8;
				fprintf(stdout, "Success!\nFile has been fully copied to the image.\n");
				return;
			}
		}
	}
	//Check for next folder to go into since not at the correct folder
	for(int i = 0; i < 16; i++){
		addr = *(unsigned short *)(p + (i*32) + offset + 11); //32 bytes per entry
		is_directory = (addr & 0x10);
		addr = *(unsigned short *)(p + (i*32) + offset + 26);
		first_logical_cluster = (addr & 0xFFFF);
		if(first_logical_cluster == 1 || first_logical_cluster == 0){
			continue;
		}
		if(is_directory != 0){
			int char_check;
			for(int j = 0; j < 8; j++){
				addr = *(unsigned short*)(p + offset + (i*32) + j);
				char_check = (addr & 0xFF);
				if(char_check == 32){
					filename[j] = '\0';
					break;
				}
				filename[j] = p[offset + (i*32) + j];
			}
			if(strncmp(filename, sub_directories[count], strlen(filename) + 1) == 0){
				dir_offset = (31 + first_logical_cluster)*bytes_per_sector;
				count++;
				search_sub_directory(dir_offset,first_logical_cluster, bytes_per_sector, p, sub_directories[count], count, num_subdirectories, first_file_cluster, date, date_acs, time);
				return;
			}
		}else{
			continue;
		}
	}
	fprintf(stdout, "Error: Directory not found!\n");
	exit(EXIT_FAILURE);
}


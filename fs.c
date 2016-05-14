/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
Arthur Pessoa de Souza
João Eduardo Brandes Luiz
Rodrigo Nascimento de Carvalho
*/


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096


unsigned short fat[65536];


typedef struct {
	char used;
	char name[25];
	unsigned short first_block;
	int size;
} dir_entry;

/*Estrutura utilizada na lista de arquivos abertos, contém modo, counter de leitura/escrita, id, index do arquivo no diretorio, current_pos de leitura e counter geral de leitura*/
typedef struct {
	char mode;
	int id;
	int index;
	int counter;
	int current_pos;
	int total;
} opened_file;

char * buffer;

dir_entry dir[128];

/*Opened directory files*/
opened_file opened_file_list[128];

/* Incremental ID variable for opened files. */
int id = 0;

/* Read blocks */
int read_block(char *sectorBuffer, int sector);

/* Write blocks */
int write_block(char *sectorBuffer, int sector);

/*
	fs_init: Function responsible to initialize the RSFS.
*/
	int fs_init() {
		int i;
		char *fatBuffer = (char *) fat;

  		/* Loads the FAT to the memory, starting from cluster 0 up to 31, 
  			the value is multiplied by 8 to obtain the value in sectors */
		for (i = 0; i < (32*8); i++) { 

			if (!bl_read (i, &fatBuffer[SECTORSIZE*i])) {
				printf ("Failure loading the FAT system!\n");
				return 0;
			}        
		}

  		/* Loading directory to memory */
		if(!read_block ((char *) dir, 32)) {
			printf ("Failure loading the directory!\n");
			return 0;
		}

  		/* Initialize opened file list, -1 = closed file */
		for (i = 0; i < 128; i++) {
			opened_file_list[i].id = -1;
			opened_file_list[i].counter = 0;
		}

		checkdisk();

		return 1;
	}

/* checkdisk - function responsible for verifying the disk integrity, 
	allocation table and root directory. */
	int checkdisk(){
		int i;

  		/* Verify if FAT is mapped correctly on disk */
		for (i = 0; i < 32; i++) { 
			if (fat[i] != 3) {
				printf("Warning: Disk is not formatted.\n");
				return 0;
			}
		}

  		/* Verify if directory is mapped on disk */
		if (fat[32] != 4){ 
			printf("Warning: Disk image contains compromised directory!\n");
			return 0;
		}
		return 1;
	}

/* fs_format: Function responsible for formatting the disk */
	int fs_format(){
		int i;

		checkdisk();

		printf("Formatting disk.\n");

  		/* Reserving space for FAT and directory */
		for(i = 0; i < 32; i++)
			fat[i] = 3;

		fat[32] = 4;

  		/* Rest of FAT initialized with 1, indicating free space */
		for (i = 33; i < 65536; i++)
			fat[i] = 1;

  		/* All directory entries initalized as non-used. */
		for (i = 0; i < 128; i++){
			dir[i].used = 0;
			strcpy(dir[i].name,"");
			dir[i].first_block=-1;
			dir[i].size=0;
		}

		update();
		return 1;
	}

/* fs_free: Funtion responsbile for counting the free space on disk. */
	int fs_free() {
		int i, count = 0;

		for (i = 33; i < (bl_size()/8); i++) {
			if (fat[i] == 1) count++;
		}

		return count * CLUSTERSIZE; 
	}

/* fs_list: Function responsible for listing all files. */
	int fs_list(char *buffer, int size) {
		int i;

		strcpy (buffer,"");

		for (i = 0; i < 128; i++) {
			if (dir[i].used == 1) {
				buffer += sprintf (buffer, "%s\t\t%d\n", dir[i].name, dir[i].size);
			}
		}

		return 1;
	}

/* fs_create: Function responsible for creating a file. */
	int fs_create(char* file_name) {
  		int i, free_entry = -1, free_block = -1;

		checkdisk();

		if (sizeof(file_name) > 24) {
			printf ("File name can't have more than 24 characters.");
			return 0;
		}

		for (i = 0; i < 128; i++) { 
			if (!strcmp (file_name, dir[i].name) && dir[i].used == 1) {
				printf ("File already exists!\n");
				return 0;
			}
			if (dir[i].used == 0 && free_entry == -1) {
				free_entry = i;
			}
		}

  		if (free_entry == -1) {
			printf ("Directory is full!\n");
			return 0;
		}

  		/* Find first free block */
		for (i = 0; i < (bl_size()/8); i++) {
			if (fat[i] == 1 && free_block == -1) {
				free_block = i;
			}
		}

  		if (free_block == -1) {
			printf("Disk is full!\n");
			return 0;
		}

		/* Creating file in the irectory */
  		dir[free_entry].used = 1;
 	 	dir[free_entry].first_block = free_block;
		strcpy(dir[free_entry].name, file_name);
  		dir[free_entry].size = 0;

  		/* Mark block as used by the new file */
		fat[free_block] = 2;

		update();
		return 1;
	} 

/* fs_remove: Function responsible for removing a file */
	int fs_remove(char *file_name) {
		
		int i, next_block, aux;
  		int first_block = -1;

		checkdisk();
  		
  		/* If file exists, first_block position receives the position stored in the directory. */
		for (i = 0; i < 128; i++) {
			if (!strcmp (dir[i].name,file_name) && dir[i].used == 1) {
				first_block = dir[i].first_block;
      			
      			/* Frees name in the directory already. */
				dir[i].used = 0;
				strcpy(dir[i].name, "");
			}
		}

  		if(first_block == -1){
			printf("File doesn't exist.");   
			return 0;
		}

		/* Solution to remove files with one block or more. Frees current block, 
			and the next one until it reaches the end of file. */
		next_block = first_block;
		while(next_block != 2){
			aux = fat[next_block];
			fat[next_block] = 1;
			next_block = aux;
		}

		update();
		return 1;
	}

/* fs_open: Function responsible for opening a file. */
	int fs_open(char *file_name, int mode) {
		
  		int i, j, first_block, first_entry;
  		char file_exists = 0;
  
  		checkdisk();

  		/* Find file */
		for (i = 0; i < 128; i++) {
			if (!strcmp (dir[i].name, file_name)) {
				first_block=dir[i].first_block;
				first_entry = i;
				file_exists=1;
			}
		}
  		
  		/* Increment id for file that will be opened */
  		id++;
  
		if (file_exists == 1) {
    		/* Find first entry of file */
			for (i = 0; i < 128; i++) {
				if (opened_file_list[i].id == -1) {
					if (mode == FS_R) {
						opened_file_list[i].mode = FS_R;
					}
        			
        			/* If write mode, rewrite file with size 0 */
					if (mode == FS_W) {
						fs_remove (file_name);
						strcpy (dir[first_entry].name, file_name);
						
						opened_file_list[i].index = first_entry;
						dir[first_entry].size = 0;
						dir[first_entry].used = 1;
						dir[first_entry].first_block = first_block;
						fat[first_block] = 2;
          				
          				update();

          				opened_file_list[i].mode = FS_W;
					}

					/* Put file on opened file list */
					opened_file_list[i].index = first_entry;
					opened_file_list[i].counter = 0;
					opened_file_list[i].total = 0;

        			/* Attribute id to opened file */
					opened_file_list[i].id = id;
					opened_file_list[i].current_pos = 0;
        			
					return opened_file_list[i].id;
				}
			}
    	} else {
			if (mode == FS_R) {
				printf("File doesn't exist");
				return -1;
			}
			
    		/* If is in write mode, we have to re-write a new file */
			for (i = 0; i < 128; i++) {
				if (opened_file_list[i].id == -1) {
					opened_file_list[i].id = id;
					if (!fs_create(file_name)) {
						return 0;
					}
        			
        			/* Find first entry of the file in directory */
					for (j = 0; j < 128; j++) {
						if (!strcmp (dir[j].name, file_name)) {
							first_entry = j;
						}
					}
        			
        			/* Put file in opened file list */
					opened_file_list[i].index = first_entry;
					opened_file_list[i].counter = 0;
					opened_file_list[i].total = 0;
					opened_file_list[i].mode = FS_W;
					opened_file_list[i].current_pos = 0;

					return opened_file_list[i].id;
				}
			}
		}
		return -1;
	}

/* fs_close: Function to close file. */
	int fs_close(int file)  {
		int i;
		
		for (i = 0; i < 128; i++) {
			if(opened_file_list[i].id == file){
				opened_file_list[i].mode = -1;
				opened_file_list[i].id = -1;
				opened_file_list[i].index = -1;
				opened_file_list[i].counter = 0;
				opened_file_list[i].current_pos = 0;
				opened_file_list[i].total = 0;
				return 1;
			}
		}

		printf("There is no file with this identifier...");
		return 0;
	}

/* fs_write: Function to write file into FAT */
	int fs_write(char *buffer, int size, int file) {
		int a, b, i, index, aux_size, id = -1;
  		
  		char* aux_file = malloc(CLUSTERSIZE*sizeof(char));

  		int aux, writeblock, next_block, last_block;
  		
  		/* Find file */
		for (i = 0; i < 128; i++) {
			if (opened_file_list[i].id == file) {
				id = i;
				index = opened_file_list[i].index;
			}
    		
    		if (opened_file_list[i].id == file && opened_file_list[i].mode == FS_R) {
				printf("File is currently in read mode.");
				return 0;
			}
		}

  		if (id == -1) {
			printf ("File isn't opened or doesn't exist.");
			return -1;
		}

  		next_block = dir[index].first_block;
  		while (next_block != 2) {
			aux = fat[next_block];
			if (aux == 2) {
				writeblock = next_block;
			}
			next_block = aux;
		}
  		
  		aux_size = dir[index].size;
		aux = 0;
  		
  		last_block = (aux_size % CLUSTERSIZE);
  		
  		if ((last_block + size) > CLUSTERSIZE) {
			/* Divide process in two parts, write first_block b, and then a which is the exceeding rest. */
			a = (last_block + size) - CLUSTERSIZE;
			b = size - a;

			if (!read_block (aux_file, writeblock)) return -1;
    		
    		/* Adding new content to the end of aux_file */
			for (i = last_block; i < last_block+b; i++) {
				aux_file[i] = buffer[aux];
				aux++;
			}

			if (!write_block (aux_file, writeblock)) return -1;
    		
    		/* Find next free block in FAT */
			for (i = 33; i < (bl_size()/8); i++) {
				if (fat[i] == 1) {
					fat[writeblock] = i;
					writeblock = i;
					fat[i] = 2;
					break;
				}
			}
    		
    		/* Clean buffer and put rest of data in it */
			strcpy (aux_file, "");
			for (i = 0; i < a; i++) {
				aux_file[i] = buffer[aux];
				aux++;
			}
    		
    		if(!write_block (aux_file, writeblock)) return -1;
    		
    		/* Update what was written up to now */
			opened_file_list[id].counter += size;
    		dir[index].size = opened_file_list[id].counter;
		} else {
			/* In case of not exceeding the size of the last block, move disk content to buffer, 
			add the new content to block and update directory. */
			if (!read_block (aux_file, writeblock)) return -1;  

			for (i = last_block; i < last_block+size; i++) {
				aux_file[i] = buffer[aux];
				aux++;
			}

			if (!write_block (aux_file, writeblock)) return -1;
			opened_file_list[id].counter += size;
			dir[index].size = opened_file_list[id].counter;
		}

		update();
		free(aux_file);
		return size;
	}

/* fs_read: Function responsible for reading a file. */
	int fs_read(char *buffer, int size, int file) {
  		int i, id = -1, index;
		int a, b;
		int aux = 0;
		int next;

		char *aux_file = malloc (CLUSTERSIZE*sizeof(char));
		
		for (i = 0; i < 128; i++) {
		  	if (opened_file_list[i].id == file) {
		  		id = i;
		  		index = opened_file_list[i].index;
		  	}
		  	if (opened_file_list[i].id == file && opened_file_list[i].mode == FS_W) {
		  		printf("File is in write mode.");
		  		return -1;
		  	}
		}

		/* If current_pos is 0, then reading hasn't started yet */		  
		if (opened_file_list[id].current_pos == 0) {
		  	opened_file_list[id].current_pos = dir[index].first_block;
		}
		  
		/*If reading a file occupying more than one block, then do: */
		if ((opened_file_list[id].counter + size) > CLUSTERSIZE) {
		    /* Same process done in the fs_write. */
		  	a = (opened_file_list[id].counter + size) - CLUSTERSIZE;
		  	b = size - a;
		    

		    if ((b + opened_file_list[id].total) >= dir[index].size) {
		    	b = dir[index].size - opened_file_list[id].total;
		    	a = 0;
		    	size = b;
		    }else if ((a + b + opened_file_list[id].total) >= dir[index].size) {
		    	a = dir[index].size - opened_file_list[id].total;
		    	size = b + a;
		    }

		    /*If size is 0, then read process is over.*/
		    if (size == 0) {
		    	return 0;
		    }

		    if(!read_block(aux_file, opened_file_list[id].current_pos))
		    	return -1;
		    
		    /*Write in read buffer what has been already read*/
		    for (i = opened_file_list[id].counter; i < opened_file_list[id].counter+b; i++) {
		    	buffer[aux] = aux_file[i];
		    	aux++;
		    }

		    next = opened_file_list[id].current_pos;
		    opened_file_list[id].current_pos = fat[next];
		    
		    if(!read_block(aux_file, opened_file_list[id].current_pos))
		    	return -1;
		    
		    opened_file_list[id].counter = 0;

		    for(i = opened_file_list[id].counter; i < opened_file_list[id].counter + a; i++) {
		    	buffer[aux] = aux_file[i];
		    	aux++;
		    }
		    /*counter receives what has been read in the beginning of the next block */
		    opened_file_list[id].counter += a;
		    /*total receives the total amount of read data*/
		    opened_file_list[id].total += a + b;
		}else{
		    if ((opened_file_list[id].total + size) >= dir[index].size) {
				size = dir[index].size - opened_file_list[id].total;
			}
		    
		    if (size == 0) {
				return 0;
			} 
		    
		    if (!read_block (aux_file, opened_file_list[id].current_pos))
				return -1;
		    
		    for (i = opened_file_list[id].counter; i < opened_file_list[id].counter+size; i++) {
				buffer[aux] = aux_file[i];
				aux++;
			}

			opened_file_list[id].counter += size;
			opened_file_list[id].total += size;
		}
		
		free(aux_file);

		return size;
	}


/* Auxiliary function */

/*write_block: Function responsible to write things in the virtual disk image.*/
int write_block(char *sectorBuffer, int sector){
	int s;
	for (s = 0; s < 8; s++) {
		/* write sectors 8 by 8, meaning only one block per time */
		if (!bl_write ((sector*8)+s, &sectorBuffer[s*SECTORSIZE])) return 0;
	}
	return 1;
}

/*read_block: Function responsible for reading a block from the virtual disk image.*/
int read_block(char *sectorBuffer, int sector){
	int s;
	for (s = 0; s < 8; s++) {
		if (!bl_read ((sector*8)+s, &sectorBuffer[SECTORSIZE*s])) return 0;
	}
	return 1;
}

/* update: Function updates all modifications made in memory for both fat and directory. */
int update(){
	int i;
  	
  	buffer = (char *) fat;

	for (i = 0; i < 32; i++)
		if(!write_block(&buffer[CLUSTERSIZE*i], i)) return 0;
  
  	buffer = (char *) dir;
  	
  	if (!write_block(buffer, 32)) return 0;
  	
  	return 1;
}

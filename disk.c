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

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "disk.h"

#define PAGESIZE 4096

int device_size;
FILE *stream;

int bl_init(char *file, int size) {
  struct stat sb;

  stream = NULL;
  if (stat(file, &sb) == 0) {
    if (S_ISREG(sb.st_mode)) {
      device_size = sb.st_size;
      stream = fopen(file, "r+");
    }
    if (stream == NULL) {
      perror("Opening existing image...");
      return 0;
    }
  } else {
    device_size = size * SECTORSIZE;
    if (device_size < 1) {
      printf("Image can't have size 0\n");
      return 0;
    }
    stream = fopen(file, "w+");
    if (stream == NULL) {
      perror("Creating new image");
      return 0;
    }
    if (truncate(file, device_size) == -1) {
      perror("Adjusting image size");
      return 0;
    }
  }
  return 1; 
}

int bl_size() {
  return device_size / SECTORSIZE;
}

int bl_write(int sector, char *buffer) {
  if (fseek(stream, sector * SECTORSIZE, SEEK_SET) == -1) {
    perror("Error positioning sector for write operation.");
    return 0;
  }
  if (fwrite(buffer, sizeof(char), SECTORSIZE, stream) != SECTORSIZE) {
    perror("Error writing sector");
    return 0;
  }
  if (fflush(stream) != 0) {
    perror("Error writing sector on disk");
    return 0;
  }
  return 1;
}

int bl_read(int sector, char *buffer){
  if (fseek(stream, sector * SECTORSIZE, SEEK_SET) == -1) {
    perror("Error positioning sector for read operation.");
    return 0;
  }
  if (fread(buffer, sizeof(char), SECTORSIZE, stream) != SECTORSIZE) {
    perror("Error reading sector");
    return 0;
  }
  return 1;
}

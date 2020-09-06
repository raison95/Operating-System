//
// Simple FIle System
// Student Name : ***
// Student Number : B******
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))		// 오른쪽에서 b+1번째 비트 1로만듬
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))	// 오른쪽에서 b+1번째 비트 0로만듬
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))		// 오른쪽에서 b+1번째 비트 뒤집음
#define BIT_CHECK(a,b) ((a) & (1<<(b)))		// 오른쪽에서 b+1번째 비트 반환

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	case -11:
		printf("%s: can't open %s input file\n",message, path); return;
	case -12:
		printf("%s: input file size exceeds the max file size\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

int find_free_block_and_set(int reset_mode,int block_num){
	int i,j,k;
	char bit_map_block[SFS_BLOCKSIZE];
	
	if(reset_mode==0){
		for(i=0;i<SFS_BITBLOCKS(spb.sp_nblocks);i++){
			disk_read(bit_map_block,SFS_MAP_LOCATION+i);
			for(j=0;j<SFS_BLOCKSIZE;j++){
				/*for(k=CHAR_BIT-1;k>=0;k--){
					if(!BIT_CHECK(bit_map_block[j],k)) {
						BIT_SET(bit_map_block[j],k);
						disk_write(bit_map_block,SFS_MAP_LOCATION+i);
						return SFS_BLOCKBITS*i+CHAR_BIT*j+(CHAR_BIT-k-1);
					}
				}*/
				for(k=0;k<CHAR_BIT;k++){
					if(!BIT_CHECK(bit_map_block[j],k)) {
						BIT_SET(bit_map_block[j],k);
						disk_write(bit_map_block,SFS_MAP_LOCATION+i);
						return SFS_BLOCKBITS*i+CHAR_BIT*j+k;
					}
				}
			}
		}
	}
	else{
		for(i=0;i<SFS_BITBLOCKS(spb.sp_nblocks);i++){
			disk_read(bit_map_block,SFS_MAP_LOCATION+i);
			for(j=0;j<SFS_BLOCKSIZE;j++){
				/*for(k=CHAR_BIT-1;k>=0;k--){
					if(SFS_BLOCKBITS*i+CHAR_BIT*j+(CHAR_BIT-k-1)==block_num) {
						BIT_CLEAR(bit_map_block[j],k);
						disk_write(bit_map_block,SFS_MAP_LOCATION+i);
						return -1;
					}
				}*/
				for(k=0;k<CHAR_BIT;k++){
					if(SFS_BLOCKBITS*i+CHAR_BIT*j+k==block_num) {
						BIT_CLEAR(bit_map_block[j],k);
						disk_write(bit_map_block,SFS_MAP_LOCATION+i);
						return -1;
					}
				}
			}
		}
	}
	
	return -1;
}

void sfs_touch(const char* path)
{
	int i,j,k;
	struct sfs_inode inode;
	struct sfs_inode new_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	struct sfs_dir new_dir_entry[SFS_DENTRYPERBLOCK];
	int free_block_num = find_free_block_and_set(0,0);
	
	// i-node 할당불가시
	if(free_block_num==-1){
		error_message("touch",path,-4);
		return;
	}
	
	disk_read(&inode,sd_cwd.sfd_ino);
	
	// path가 이미 있는 경우
	for(i=0; i < SFS_NDIRECT; i++) {
		if (inode.sfi_direct[i] == 0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(!strcmp(dir_entry[j].sfd_name,path)){
				find_free_block_and_set(1,free_block_num);
				error_message("touch",path,-6);
				return;
			}
		}
	}

	// 엔트리가 꽉찬 경우
	if(inode.sfi_size==SFS_NDIRECT*SFS_DENTRYPERBLOCK){
		find_free_block_and_set(1,free_block_num);
		error_message("touch",path,-3);
		return;
	}
	
	for(i=0; i < SFS_NDIRECT; i++) {
		if (inode.sfi_direct[i] == 0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(dir_entry[j].sfd_ino==SFS_NOINO){
				dir_entry[j].sfd_ino=free_block_num;
				strcpy(dir_entry[j].sfd_name,path);
				disk_write(dir_entry, inode.sfi_direct[i]);
				inode.sfi_size += sizeof(struct sfs_dir);
				disk_write(&inode,sd_cwd.sfd_ino);	
				
				new_inode.sfi_size=0;
				new_inode.sfi_type=SFS_TYPE_FILE;
				for(k=0;k<SFS_NDIRECT;k++){
					new_inode.sfi_direct[k]=0;
				}
				new_inode.sfi_indirect=0;
				disk_write(&new_inode,free_block_num);	
				
				return;
			}
		}
	}
}

void sfs_cd(const char* path)
{
	int i,j;
	int toggle=0;
	struct sfs_inode inode;
	struct sfs_inode temp_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	
	if(path==NULL) {
		sd_cwd.sfd_ino = 1;		
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
	}
	else{
		disk_read(&inode,sd_cwd.sfd_ino);
		for(i=0; i < SFS_NDIRECT; i++) {
			if(inode.sfi_direct[i]==0) continue;
			disk_read(dir_entry, inode.sfi_direct[i]);
			for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
				if(!strcmp(dir_entry[j].sfd_name,path)){
					toggle=1;
					disk_read(&temp_inode, dir_entry[j].sfd_ino);
					if(temp_inode.sfi_type == SFS_TYPE_FILE){
						error_message("cd",path,-2);
						return;
					}
					else if(temp_inode.sfi_type == SFS_TYPE_DIR){
						sd_cwd.sfd_ino = dir_entry[j].sfd_ino;
						strcpy(sd_cwd.sfd_name,path);
						return;
					}
				}
			}
		}
		if(toggle==0) {
				error_message("cd",path,-1);
				return;
		}
	}
}

void print_ls(struct sfs_dir dir_entry[])
{
	int i;
	struct sfs_inode inode;
	
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("%s",dir_entry[i].sfd_name);
			printf("\t");
		}
		else if(inode.sfi_type == SFS_TYPE_DIR) {
			printf("%s",dir_entry[i].sfd_name);
			printf("/");
			printf("\t");
		}
	}
	printf("\n");
}

void sfs_ls(const char* path)
{
	int i,j,k;
	int toggle=0;
	struct sfs_inode c_inode;
	struct sfs_inode temp_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	
	disk_read(&c_inode, sd_cwd.sfd_ino);
	
	if(path==NULL) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (c_inode.sfi_direct[i] == 0) continue;
			disk_read(dir_entry, c_inode.sfi_direct[i]);
			print_ls(dir_entry);
		}
	}
	else{
		for(i=0; i < SFS_NDIRECT; i++) {
			if (c_inode.sfi_direct[i] == 0) continue;
			disk_read(dir_entry, c_inode.sfi_direct[i]);
			for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
				if(!strcmp(dir_entry[j].sfd_name,path)){
					toggle=1;
					disk_read(&temp_inode, dir_entry[j].sfd_ino);
					if(temp_inode.sfi_type == SFS_TYPE_FILE){
						printf("%s",path);
						printf("\n");
						return;
					}
					else if(temp_inode.sfi_type == SFS_TYPE_DIR){
						for(k=0; k < SFS_NDIRECT; k++) {
							if (temp_inode.sfi_direct[k] == 0) continue;
							disk_read(dir_entry, temp_inode.sfi_direct[k]);
							print_ls(dir_entry);
						}
						return;
					}
				}
			}
		}
		if(toggle==0){
			error_message("ls",path,-1);
			return;
		}
	}
}

void sfs_mkdir(const char* org_path) 
{
	int i,j,k;
	struct sfs_inode inode;
	struct sfs_inode new_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	struct sfs_dir new_dir_entry[SFS_DENTRYPERBLOCK];
	int free_block_num = find_free_block_and_set(0,0);
	int free_block_num_2 = find_free_block_and_set(0,0);
	
	// i-node 할당불가시
	if(free_block_num==-1){
		error_message("mkdir",org_path,-4);
		return;
	}
	
	// i-node할당 후 디렉토리 엔트리블록할당시
	if(free_block_num_2==-1){
		find_free_block_and_set(1,free_block_num);
		error_message("mkdir",org_path,-4);
		return;
	}
	
	disk_read(&inode,sd_cwd.sfd_ino);
	
	// path가 이미 있는 경우
	for(i=0; i < SFS_NDIRECT; i++) {
		if (inode.sfi_direct[i] == 0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(!strcmp(dir_entry[j].sfd_name,org_path)){
				find_free_block_and_set(1,free_block_num);
				find_free_block_and_set(1,free_block_num_2);
				error_message("mkdir",org_path,-6);
				return;
			}
		}
	}

	// 엔트리가 꽉찬 경우
	if(inode.sfi_size==SFS_NDIRECT*SFS_DENTRYPERBLOCK){
		find_free_block_and_set(1,free_block_num);
		find_free_block_and_set(1,free_block_num_2);
		error_message("mkdir",org_path,-3);
		return;
	}
	
	for(i=0; i < SFS_NDIRECT; i++) {
		if (inode.sfi_direct[i] == 0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(dir_entry[j].sfd_ino==SFS_NOINO){
				dir_entry[j].sfd_ino=free_block_num;
				strcpy(dir_entry[j].sfd_name,org_path);
				disk_write(dir_entry, inode.sfi_direct[i]);
				inode.sfi_size += sizeof(struct sfs_dir);
				disk_write(&inode,sd_cwd.sfd_ino);	
				
				new_inode.sfi_size=2*sizeof(struct sfs_dir);
				new_inode.sfi_type=SFS_TYPE_DIR;
				new_inode.sfi_direct[0]=free_block_num_2;
				for(k=1;k<SFS_NDIRECT;k++){
					new_inode.sfi_direct[k]=0;
				}
				new_inode.sfi_indirect=0;
				disk_write(&new_inode,free_block_num);	
				
				new_dir_entry[0].sfd_ino = free_block_num;
				new_dir_entry[0].sfd_name[0] = '.';
				new_dir_entry[0].sfd_name[1] = '\0';
				new_dir_entry[1].sfd_ino = sd_cwd.sfd_ino;
				new_dir_entry[1].sfd_name[0] = '.';
				new_dir_entry[1].sfd_name[1] = '.';
				new_dir_entry[0].sfd_name[2] = '\0';
				for(k=2; k < SFS_DENTRYPERBLOCK; k++) {
					new_dir_entry[k].sfd_ino = SFS_NOINO;
					new_dir_entry[k].sfd_name[0] = 'f';
					new_dir_entry[k].sfd_name[1] = '\0';
				}
				disk_write(new_dir_entry,free_block_num_2);
				return;
			}
		}
	}
}

void sfs_rmdir(const char* org_path) 
{
	int i,j,k;
	int toggle=0;
	int dir_empty=0;
	struct sfs_inode inode;
	struct sfs_inode temp_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	char bit_map_block[SFS_BLOCKSIZE];
	
	if(!strcmp(org_path,".")){
		error_message("rmdir",org_path,-8);
		return;
	}
	disk_read(&inode, sd_cwd.sfd_ino);
	
	for(i=0;i<SFS_NDIRECT;i++) {
		if(inode.sfi_direct[i]==0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(!strcmp(dir_entry[j].sfd_name,org_path)){
				toggle=1;
				disk_read(&temp_inode, dir_entry[j].sfd_ino);
				if(temp_inode.sfi_type == SFS_TYPE_FILE){
					error_message("rmdir",org_path,-5);
					return;
				}
				else if(temp_inode.sfi_type == SFS_TYPE_DIR){
					if(temp_inode.sfi_size!=sizeof(struct sfs_dir)*2){
						error_message("rmdir",org_path,-7);
						return;
					}
					
					disk_read(bit_map_block,SFS_MAP_LOCATION+(temp_inode.sfi_direct[0]/SFS_BLOCKBITS));
					BIT_CLEAR(bit_map_block[(temp_inode.sfi_direct[0]%SFS_BLOCKBITS)/CHAR_BIT],(temp_inode.sfi_direct[0]%SFS_BLOCKBITS)%CHAR_BIT);
					disk_write(bit_map_block,SFS_MAP_LOCATION+(temp_inode.sfi_direct[0]/SFS_BLOCKBITS));
					
					disk_read(bit_map_block,SFS_MAP_LOCATION+(dir_entry[j].sfd_ino/SFS_BLOCKBITS));
					BIT_CLEAR(bit_map_block[(dir_entry[j].sfd_ino%SFS_BLOCKBITS)/CHAR_BIT],(dir_entry[j].sfd_ino%SFS_BLOCKBITS)%CHAR_BIT);
					disk_write(bit_map_block,SFS_MAP_LOCATION+(dir_entry[j].sfd_ino/SFS_BLOCKBITS));
					
					dir_entry[j].sfd_ino=SFS_NOINO;
					dir_entry[j].sfd_name[0]='f';
					dir_entry[j].sfd_name[1]='\0';
					disk_write(dir_entry, inode.sfi_direct[i]);
					inode.sfi_size-=sizeof(struct sfs_dir);
					disk_write(&inode, sd_cwd.sfd_ino);
					
					return;
				}
			}
		}
	}
	if(toggle==0){
		error_message("rmdir",org_path,-1);
	}
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	int i,j,k,l;
	int toggle=0;
	struct sfs_inode inode;
	struct sfs_inode temp_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	
	disk_read(&inode, sd_cwd.sfd_ino);
	
	for(i=0;i<SFS_NDIRECT;i++) {
		if(inode.sfi_direct[i]==0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(!strcmp(dir_entry[j].sfd_name,src_name)){
				toggle=1;
				
				for(k=0;k<SFS_NDIRECT;k++){
					if(inode.sfi_direct[k]==0) continue;
					disk_read(dir_entry, inode.sfi_direct[k]);
					for(l=0; l < SFS_DENTRYPERBLOCK; l++) {
						if(!strcmp(dir_entry[l].sfd_name,dst_name)){
							error_message("mv",dst_name,-6);
							return;
						}
					}
				}
				
				strcpy(dir_entry[j].sfd_name,dst_name);
				disk_write(dir_entry, inode.sfi_direct[i]);
				
				return;
			}
		}
	}
				
	if(toggle==0) {
		error_message("mv",src_name,-1);
	}
}

void sfs_rm(const char* path) 
{
	int i,j,k;
	int toggle=0;
	int dir_empty=0;
	int temp_block_num;
	struct sfs_inode inode;
	struct sfs_inode temp_inode;
	char temp_inode_2[SFS_BLOCKSIZE];
	int inode_2[SFS_DBPERIDB];
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	char bit_map_block[SFS_BLOCKSIZE];
	
	disk_read(&inode, sd_cwd.sfd_ino);
	
	for(i=0;i<SFS_NDIRECT;i++) {
		if(inode.sfi_direct[i]==0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(!strcmp(dir_entry[j].sfd_name,path)){
				toggle=1;
				disk_read(&temp_inode, dir_entry[j].sfd_ino);
				if(temp_inode.sfi_type == SFS_TYPE_DIR){
					error_message("rm",path,-9);
					return;
				}
				else if(temp_inode.sfi_type == SFS_TYPE_FILE){
					for(k=0;k<SFS_NDIRECT;k++){
						if(temp_inode.sfi_direct[k]==0) continue;
						disk_read(bit_map_block,SFS_MAP_LOCATION+(temp_inode.sfi_direct[k]/SFS_BLOCKBITS));
						BIT_CLEAR(bit_map_block[(temp_inode.sfi_direct[k]%SFS_BLOCKBITS)/CHAR_BIT],(temp_inode.sfi_direct[k]%SFS_BLOCKBITS)%CHAR_BIT);
						disk_write(bit_map_block,SFS_MAP_LOCATION+(temp_inode.sfi_direct[k]/SFS_BLOCKBITS));
					}
					
					disk_read(temp_inode_2, temp_inode.sfi_indirect);
					memcpy(inode_2,temp_inode_2,512);
					
					for(k=0;k<SFS_BLOCKSIZE;k++){
						if(temp_inode.sfi_indirect==0) break;
						temp_block_num=inode_2[k];

						disk_read(bit_map_block,SFS_MAP_LOCATION+(temp_block_num/SFS_BLOCKBITS));
						BIT_CLEAR(bit_map_block[(temp_block_num%SFS_BLOCKBITS)/CHAR_BIT],(temp_block_num%SFS_BLOCKBITS)%CHAR_BIT);
						disk_write(bit_map_block,SFS_MAP_LOCATION+(temp_block_num/SFS_BLOCKBITS));
					}
										
					disk_read(bit_map_block,SFS_MAP_LOCATION+(dir_entry[j].sfd_ino/SFS_BLOCKBITS));
					BIT_CLEAR(bit_map_block[(dir_entry[j].sfd_ino%SFS_BLOCKBITS)/CHAR_BIT],(dir_entry[j].sfd_ino%SFS_BLOCKBITS)%CHAR_BIT);
					disk_write(bit_map_block,SFS_MAP_LOCATION+(dir_entry[j].sfd_ino/SFS_BLOCKBITS));
					
					dir_entry[j].sfd_ino=SFS_NOINO;
					dir_entry[j].sfd_name[0]='f';
					dir_entry[j].sfd_name[1]='\0';
					disk_write(dir_entry, inode.sfi_direct[i]);
					inode.sfi_size-=sizeof(struct sfs_dir);
					disk_write(&inode, sd_cwd.sfd_ino);
					
					return;
				}
			}
		}
	}
	if(toggle==0){
		error_message("rm",path,-1);
	}
}

void sfs_cpin(const char* local_path, const char* path) 
{
	int i,j,k,l;
	int cnt=0;
	int tot=0;
	FILE*fp=fopen(path,"rb");
	FILE*temp=fopen(path,"rb");
	char block_buf_2[SFS_BLOCKSIZE*(15+(128+1))];
	char block_buf[SFS_BLOCKSIZE];
	struct sfs_inode inode;
	struct sfs_inode new_inode;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];
	struct sfs_dir new_dir_entry[SFS_DENTRYPERBLOCK];
	int free_block_num = find_free_block_and_set(0,0);
	char file_temp_buf[SFS_BLOCKSIZE];
	int file_buf[SFS_DBPERIDB];
	int block_num;
	
	if(fp==NULL){
		error_message("cpin",path,-11);
		return;
	}
	
	for(i=0;i<15+128;i++){
		cnt=fread(block_buf_2,512,1,temp);
		tot+=cnt;
	}	
	
	if(!feof(temp)) {
			error_message("cpin",path,-12);
			return;
	}
	fclose(temp);
	i=0;cnt=0;tot=0;
	fseek(temp, 0, SEEK_END);
	
	// i-node 할당불가시
	if(free_block_num==-1){
		error_message("cpin",local_path,-4);
		return;
	}
	
	disk_read(&inode,sd_cwd.sfd_ino);
	
	// path가 이미 있는 경우
	for(i=0; i < SFS_NDIRECT; i++) {
		if (inode.sfi_direct[i] == 0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(!strcmp(dir_entry[j].sfd_name,local_path)){
				find_free_block_and_set(1,free_block_num);
				error_message("cpin",local_path,-6);
				return;
			}
		}
	}

	// 엔트리가 꽉찬 경우
	if(inode.sfi_size==SFS_NDIRECT*SFS_DENTRYPERBLOCK){
		find_free_block_and_set(1,free_block_num);
		error_message("cpin",local_path,-3);
		return;
	}
	
	for(i=0; i < SFS_NDIRECT; i++) {
		if (inode.sfi_direct[i] == 0) continue;
		disk_read(dir_entry, inode.sfi_direct[i]);
		for(j=0; j < SFS_DENTRYPERBLOCK; j++) {
			if(dir_entry[j].sfd_ino==SFS_NOINO){
				dir_entry[j].sfd_ino=free_block_num;
				strcpy(dir_entry[j].sfd_name,local_path);
				disk_write(dir_entry, inode.sfi_direct[i]);
				inode.sfi_size += sizeof(struct sfs_dir);
				disk_write(&inode,sd_cwd.sfd_ino);	
				
				new_inode.sfi_size=0;
				new_inode.sfi_type=SFS_TYPE_FILE;
				for(k=0;k<SFS_NDIRECT;k++){
					if(feof(fp)) {
						find_free_block_and_set(1,new_inode.sfi_direct[k]);
						break;
					}
					new_inode.sfi_direct[k]=find_free_block_and_set(0,0);
					if(new_inode.sfi_direct[k]==-1) {
						error_message("cpin",local_path,-4);
						return;
					}
					disk_read(block_buf,new_inode.sfi_direct[k]);
					cnt=fread(block_buf,1,512,fp);
					tot+=cnt;
					disk_write(block_buf,new_inode.sfi_direct[k]);
				}
				if(!feof(fp)){
					new_inode.sfi_indirect=find_free_block_and_set(0,0);
					if(new_inode.sfi_indirect==-1) {
							error_message("cpin",local_path,-4);
							return;
					}
					disk_read(file_temp_buf,new_inode.sfi_indirect);
					for(k=0;k<SFS_DBPERIDB;k++){
						if(feof(fp)) {
							find_free_block_and_set(1,block_num);
							break;
						}
						block_num=find_free_block_and_set(0,0);
						if(block_num==-1) {
							error_message("cpin",local_path,-4);
							return;
						}
						disk_read(block_buf,block_num);
						file_buf[k]=block_num;
						cnt=fread(block_buf,1,512,fp);					
						tot+=cnt;					
						disk_write(block_buf,block_num);
					}
					memcpy(file_temp_buf,file_buf,512);
					disk_write(file_temp_buf,new_inode.sfi_indirect);
				}
				new_inode.sfi_size=tot;
				disk_write(&new_inode,free_block_num);	
				fclose(fp);
				return;
			}
		}
	}
	fclose(fp);
}

void sfs_cpout(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}

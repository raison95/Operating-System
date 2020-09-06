//
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
// Submission Year:2019
// Student Name:***
// Student Number:B******
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct pageTableEntry {
	int level;			// page table level (1 or 2)
	char valid;
	struct pageTableEntry *secondLevelPageTable;	// valid if this entry is for the first level page table  (level = 1)
	int frameNumber;				// valid if this entry is for the second level page table (level = 2)
};

struct framePage {
	int number;			// frame number
	int pid;			// Process id that owns the frame
	int virtualPageNumber;		// virtual page number using the frame
	struct framePage *lruLeft;	// for LRU circular doubly linked list
	struct framePage *lruRight;     // for LRU circular doubly linked list
};

struct invertedPageTableEntry {
	int pid;			// process id
	int virtualPageNumber;		// virtual page number
	int frameNumber;		// frame number allocated
	struct invertedPageTableEntry *next;
};

struct procEntry {
	char *traceName;		// the memory trace name
	int pid;			// process (trace) id
	int ntraces;			// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;		// The number of page faults
	int numPageHit;			// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
};

struct framePage *oldestFrame; 		// the oldest frame pointer
int simType, firstLevelBits, phyMemSizeBits, numProcess;
int s_flag = 0;
int nFrame;

void initProcTableEntry(struct procEntry *procTable,int simType){
	int i,j,tablesize;
	// init procTable
	for(i=0;i<numProcess;i++){
		//free(procTable[i].firstLevelPageTable);
		procTable[i].pid=i;
                procTable[i].ntraces=0;
                procTable[i].num2ndLevelPageTable=0;
                procTable[i].numIHTConflictAccess=0;
                procTable[i].numIHTNULLAccess=0;
                procTable[i].numIHTNonNULLAcess=0;
		procTable[i].numPageFault=0;
		procTable[i].numPageHit=0;
		if(simType==0) tablesize=1<<20;
		if(simType==1) tablesize=1<<firstLevelBits;
		procTable[i].firstLevelPageTable=malloc(sizeof(struct pageTableEntry)*(tablesize));
                for(j=0;j<tablesize;j++){
                       	procTable[i].firstLevelPageTable[j].level=1;
                       	procTable[i].firstLevelPageTable[j].valid='N';
			procTable[i].firstLevelPageTable[j].secondLevelPageTable=NULL;	
			procTable[i].firstLevelPageTable[j].frameNumber=-1;
                }
	}
}			

void initPhyMem(struct framePage *phyMem, int nFrame) {
	int i;
	for(i = 0; i < nFrame; i++) {
		phyMem[i].number = i;
		phyMem[i].pid = -1;
		phyMem[i].virtualPageNumber = -1;
		phyMem[i].lruLeft = &phyMem[(i-1+nFrame) % nFrame];
		phyMem[i].lruRight = &phyMem[(i+1+nFrame) % nFrame];
	}

	oldestFrame = &phyMem[0];

}

void oneLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, char FIFOorLRU) {
	int i,j=0;
	unsigned Vaddr,Paddr,pageidx;
	char rw;
	int end_state=0;
	struct framePage *current=phyMemFrames;
	struct framePage *temp;
	while(1){		
		for(i = 0; i < numProcess; i++) {
			fscanf(procTable[i].tracefp,"%x %c",&Vaddr,&rw);
			if(feof(procTable[i].tracefp)) {
				end_state=1;
				break;
			}
			pageidx = Vaddr>>12;
			if(procTable[i].firstLevelPageTable[pageidx].valid=='Y'){
				procTable[i].numPageHit++;
				if(FIFOorLRU=='L'){
					if(j==(nFrame)){
						if(!((current->pid==i)&&(current->virtualPageNumber==pageidx))){
							if(!((oldestFrame->pid==i)&&(oldestFrame->virtualPageNumber==pageidx))){
								/*for(temp=oldestFrame->lruRight;temp!=current;temp=temp->lruRight){
									if((temp->pid==i)&&(temp->virtualPageNumber==pageidx)){
										temp->lruLeft->lruRight=temp->lruRight;
                                                				temp->lruRight->lruLeft=temp->lruLeft;
                                                				temp->lruLeft=current;
                                                				current->lruRight->lruLeft=temp;
                                                				temp->lruRight=current->lruRight;
                                                				current->lruRight=temp;
										current=temp;
										break;
									}
								}*/
								temp=&phyMemFrames[procTable[i].firstLevelPageTable[pageidx].frameNumber];
								
								temp->lruLeft->lruRight=temp->lruRight;
                                                                temp->lruRight->lruLeft=temp->lruLeft;
                                                                temp->lruLeft=current;
                                                                current->lruRight->lruLeft=temp;
                                                                temp->lruRight=current->lruRight;
                                                                current->lruRight=temp;
                                                                
								current=temp;
							}	
							else{
								current=current->lruRight;
								oldestFrame=oldestFrame->lruRight;
							}
						}
					}
				}
			}	
			else{
				procTable[i].numPageFault++;
				if(FIFOorLRU=='F'){
					if(current->pid!=-1) {
						procTable[current->pid].firstLevelPageTable[current->virtualPageNumber].valid='N';		
					}
					current->pid=i;
					current->virtualPageNumber=pageidx;
					procTable[i].firstLevelPageTable[pageidx].frameNumber=current->number;
					procTable[i].firstLevelPageTable[pageidx].valid='Y';
					current=current->lruRight;
				}
				if(FIFOorLRU=='L'){
					if(j==(nFrame)) {
						procTable[oldestFrame->pid].firstLevelPageTable[oldestFrame->virtualPageNumber].valid='N';
						oldestFrame->pid=i;
						oldestFrame->virtualPageNumber=pageidx;
                                                procTable[i].firstLevelPageTable[pageidx].frameNumber=oldestFrame->number;
                                                procTable[i].firstLevelPageTable[pageidx].valid='Y';
						current=oldestFrame;
						//current=current->lruRight;
						oldestFrame=oldestFrame->lruRight;
					}
					else{	
						++j;
						current->pid=i;
						current->virtualPageNumber=pageidx;
						procTable[i].firstLevelPageTable[pageidx].frameNumber=current->number;
						procTable[i].firstLevelPageTable[pageidx].valid='Y';
						current=current->lruRight;
						if(j==(nFrame)) current=current->lruLeft;
					}
				}
			}		
			Paddr=((procTable[i].firstLevelPageTable[pageidx].frameNumber)<<12)+((Vaddr<<20)>>20);
			procTable[i].ntraces++;
			// -s option print statement
			if(s_flag) printf("One-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces,Vaddr,Paddr );
		}		
		if(end_state) break;
	}
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		free(procTable[i].firstLevelPageTable);
		rewind(procTable[i].tracefp);
	}
}

void twoLevelVMSim(struct procEntry *procTable, struct framePage *phyMemFrames) {	
        int i,j;
	char rw;
	unsigned temp;
        unsigned Vaddr,Paddr,pageidx_1,pageidx_2,pageNum;
        unsigned firstTableSize=1<<firstLevelBits;
	unsigned SecondTableSize=1<<(20-firstLevelBits);
	int hit_fault_state=0;
        int end_state=0;
        struct framePage *current=phyMemFrames;

	while(1){
                for(i = 0; i < numProcess; i++) {
                        fscanf(procTable[i].tracefp,"%x %c",&Vaddr,&rw);
                        if(feof(procTable[i].tracefp)) {
                                end_state=1;
                                break;
                        }
                        temp=0xffffffff;
			pageidx_1 = Vaddr>>(32-firstLevelBits);
                        pageidx_2 = ((Vaddr<<firstLevelBits)>>(firstLevelBits+12));
			pageNum = Vaddr>>12;
                        hit_fault_state=0;
			//puts("1");
			if(procTable[i].firstLevelPageTable[pageidx_1].valid=='N'){
				procTable[i].num2ndLevelPageTable++;
				procTable[i].firstLevelPageTable[pageidx_1].valid='Y';
				procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable=malloc(sizeof(struct pageTableEntry)*SecondTableSize);
				for(j=0;j<SecondTableSize;j++){
					procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[j].level=1;
					procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[j].valid='N';
					procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[j].frameNumber=-1;
				}
			}
			// puts("2");		
                        if((procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].valid=='Y')&&(phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].pid==i)){
                                        if((phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].virtualPageNumber==pageNum)){
                                                phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].lruLeft->lruRight=phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].lruRight;
                                                phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].lruRight->lruLeft=phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].lruLeft;
                                                phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].lruRight=current;
                                                current->lruLeft->lruRight=&phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber];
                                                phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber].lruLeft=current->lruLeft;
                                                current->lruLeft=&phyMemFrames[procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber];
                                                hit_fault_state=1;
                                        }
                                        else{
                                                for(j=0;j<nFrame;j++)
                                                        if(phyMemFrames[j].virtualPageNumber==pageNum) {
                                                                phyMemFrames[j].lruLeft->lruRight=phyMemFrames[j].lruRight;
                                                                phyMemFrames[j].lruRight->lruLeft=phyMemFrames[j].lruLeft;
                                                                phyMemFrames[j].lruRight=current;
                                                                current->lruLeft->lruRight=&phyMemFrames[j];
                                                                phyMemFrames[j].lruLeft=current->lruLeft;
                                                                current->lruLeft=&phyMemFrames[j];
                                            	        	hit_fault_state=1;
                                                        	break;
							}
                                        }
                       	}
			// puts("3");	
                        if(hit_fault_state){
                                procTable[i].numPageHit++;
                                Paddr = ((current->lruLeft->number)<<12)+((Vaddr<<20)>>20);
                        }
                        else{
			// puts("4");
              			procTable[i].numPageFault++; 
                                if(current->pid==-1){
					 //puts("5");
                                        current->pid=i;
                                        current->virtualPageNumber=pageNum;
                                        procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber=current->number;
                                        procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].valid='Y';
                                        current=current->lruRight;
                                        Paddr = ((current->lruLeft->number)<<12)+((Vaddr<<20)>>20);
                                }
                                else{
					 //puts("6");
					//printf("%d %x %x\n",oldestFrame->pid,(oldestFrame->virtualPageNumber>>(20-firstLevelBits))&(temp>>(32-firstLevelBits)),(oldestFrame->virtualPageNumber)&(temp>>(firstLevelBits+12)));


					procTable[oldestFrame->pid].firstLevelPageTable[(oldestFrame->virtualPageNumber>>(20-firstLevelBits))&(temp>>(32-firstLevelBits))].secondLevelPageTable[(oldestFrame->virtualPageNumber)&(temp>>(firstLevelBits+12))].valid='N';
                                        //procTable[oldestFrame->pid].firstLevelPageTable[oldestFrame->virtualPageNumber>>(20-firstLevelBits)].valid='N';
                                         // puts("7");
					oldestFrame->pid=i;
                                        //  puts("8");
					oldestFrame->virtualPageNumber=pageNum;
                                        //  puts("9");
					procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].frameNumber=oldestFrame->number;
                                        //  puts("10");
					procTable[i].firstLevelPageTable[pageidx_1].secondLevelPageTable[pageidx_2].valid='Y';
                                        //  puts("11");
					oldestFrame=oldestFrame->lruRight;
                                        // puts("12");
					Paddr =((oldestFrame->lruLeft->number)<<12)+((Vaddr<<20)>>20);
                                }
                         puts("4");
			}			
			procTable[i].ntraces++;
			// -s option print statement
			if(s_flag) printf("Two-Level procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces,Vaddr,Paddr);
		}
		if(end_state) break;
	}
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		//free(procTable[i].firstLevelPageTable;
		rewind(procTable[i].tracefp);
	}
}

void invertedPageVMSim(struct procEntry *procTable, struct framePage *phyMemFrames, int nFrame) {	
	struct invertedPageTableEntry *invertedTable = malloc(sizeof(struct invertedPageTableEntry)*nFrame);
	struct invertedPageTableEntry *temp;
	struct invertedPageTableEntry *before;
	int i,j;
        char rw;
        unsigned Vaddr,Paddr,pageidx,pageNum,frameNum;
        int hit_fault_state=0;
        int end_state=0;
        struct framePage *current=phyMemFrames;
	unsigned pivot;

        for(i=0;i<nFrame;i++){
		invertedTable[i].pid=-1;
		invertedTable[i].virtualPageNumber=-1;
		invertedTable[i].frameNumber=-1;
		invertedTable[i].next=malloc(sizeof(struct invertedPageTableEntry));	
	}
	while(1){
                for(i = 0; i < numProcess; i++) {
                        fscanf(procTable[i].tracefp,"%x %c",&Vaddr,&rw);
                        if(feof(procTable[i].tracefp)) {
                                end_state=1;
                                break;
                        }
                        pageNum = Vaddr>>12;
			pageidx = (pageNum+i)%nFrame;
			hit_fault_state=0;
			before=&invertedTable[pageidx];
			for(temp=invertedTable[pageidx].next;temp!=NULL;temp=temp->next){
				if((temp->pid==i)&&(temp->virtualPageNumber==pageNum)){
					frameNum=temp->frameNumber;
					if(temp->next!=NULL){
						before->next=temp->next;
						temp->next=invertedTable[pageidx].next;
						invertedTable[pageidx].next=temp;
					}
					else{
						before->next=NULL;
				                temp->next=invertedTable[pageidx].next;
                                                invertedTable[pageidx].next=temp;
					}
					hit_fault_state=1;
					break;	
				}
				before=temp;
			}
			for(j=0;j<nFrame;j++)
                        	if(phyMemFrames[j].virtualPageNumber==pageNum) {
                                	frameNum=j;
					phyMemFrames[j].lruLeft->lruRight=phyMemFrames[j].lruRight;
                                        phyMemFrames[j].lruRight->lruLeft=phyMemFrames[j].lruLeft;
                                        phyMemFrames[j].lruRight=current;
                                        current->lruLeft->lruRight=&phyMemFrames[j];
                                        phyMemFrames[j].lruLeft=current->lruLeft;
                                        current->lruLeft=&phyMemFrames[j];
                                        hit_fault_state=1;
					break;
				}			
                        if(hit_fault_state){
				procTable[i].numPageHit++;
				procTable[i].numIHTConflictAccess++;
				procTable[i].numIHTNonNULLAcess++;
				//printf("zzzzz:%x\n",frameNum);
				Paddr = (frameNum<<12)+((Vaddr<<20)>>20);
			}
		        else{
                                procTable[i].numPageFault++;
                                procTable[i].numIHTNULLAccess++;
				if(current->pid==-1){
                                        //puts("1");
					current->pid=i;
                                        current->virtualPageNumber=pageNum;					
					temp=malloc(sizeof(struct invertedPageTableEntry));
					temp->next=invertedTable[pageidx].next;
					invertedTable[pageidx].next=temp;
					temp->pid=i;
					temp->virtualPageNumber=pageNum;
					temp->frameNumber=current->number;
					current=current->lruRight;
                                        Paddr = ((temp->frameNumber)<<12)+((Vaddr<<20)>>20);
				}
                                else{
					//puts("2");
                                        for(before=&invertedTable[pageidx],temp=invertedTable[pageidx].next;temp->next!=NULL;temp=temp->next){
						if((temp->pid==oldestFrame->pid)&&(temp->virtualPageNumber==oldestFrame->virtualPageNumber)){
							if(temp->next!=NULL){
							        before->next=temp->next;
							}
							else{
							        before->next==NULL;
							}	
						}
						before=temp;
						free(temp);
					}
					oldestFrame->pid=i;
                                        oldestFrame->virtualPageNumber=pageNum;
					oldestFrame=oldestFrame->lruRight;
                                        Paddr =((oldestFrame->lruLeft->number)<<12)+((Vaddr<<20)>>20);
				}
                        }
                        procTable[i].ntraces++;
                        // -s option print statement
                	if(s_flag) printf("IHT procID %d traceNumber %d virtual addr %x physical addr %x\n", i, procTable[i].ntraces,Vaddr,Paddr);
		}
                if(end_state) break;
        }
		
	// -s option print statement	
	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
		rewind(procTable[i].tracefp);
	}
}

int main(int argc, char *argv[]) {
	int i,j;
	int firstTableSize;
	struct procEntry *procTable;
	struct framePage *phyMemFrames;
	if(!strcmp(argv[1],"-s")){
		s_flag=1;
		simType=atoi(argv[2]);firstLevelBits=atoi(argv[3]);phyMemSizeBits=atoi(argv[4]);
		numProcess=argc-5;
	}
	else{
		s_flag=0;
		simType=atoi(argv[1]);firstLevelBits=atoi(argv[2]);phyMemSizeBits=atoi(argv[3]);
		numProcess=argc-4;
	}
	
	nFrame = (1<<(phyMemSizeBits-PAGESIZEBITS));assert(nFrame>0);
	firstTableSize = 1<<firstLevelBits;
	procTable = malloc(sizeof(struct procEntry)*numProcess);
	phyMemFrames = malloc(sizeof(struct framePage)*nFrame);
	
	if (0/**/) {
	     printf("Usage : %s [-s] simType firstLevelBits PhysicalMemorySizeBits TraceFileNames\n",argv[0]); exit(1);
	}
	if (phyMemSizeBits < PAGESIZEBITS) {
		printf("PhysicalMemorySizeBits %d should be larger than PageSizeBits %d\n",phyMemSizeBits,PAGESIZEBITS); exit(1);
	}
	if (VIRTUALADDRBITS - PAGESIZEBITS - firstLevelBits <= 0 ) {
		printf("firstLevelBits %d is too Big for the 2nd level page system\n",firstLevelBits); exit(1);
	}
	if(simType<0){
      		puts("simType cannot be a negative number");
        	return -1;
	}
		
	// initialize procTable for memory simulations
	for(i = 0; i < numProcess; i++) {
		// opening a tracefile for the process
		if(s_flag) {
			procTable[i].traceName = argv[i+5];
			procTable[i].tracefp = fopen(argv[i+5],"r");
			printf("process %d opening %s\n",i,argv[i+5]);
		}
		else {
			procTable[i].traceName = argv[i+4];
			procTable[i].tracefp = fopen(argv[i+4],"r");
			printf("process %d opening %s\n",i,argv[i+4]);
		}
	}


	printf("\nNum of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));
	
	// initialize procTable for the simulation
	for(i = 0; i < numProcess; i++) {
		// rewind tracefiles
		rewind(procTable[i].tracefp);	
	}
	
	switch(simType){
		case 0:
			printf("=============================================================\n");
			printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with FIFO
			initProcTableEntry(procTable,0);
			initPhyMem(phyMemFrames,nFrame);	
			oneLevelVMSim(procTable,phyMemFrames,'F');
			printf("=============================================================\n");
			printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with LRU
                        initProcTableEntry(procTable,0);
                        initPhyMem(phyMemFrames,nFrame);
			oneLevelVMSim(procTable,phyMemFrames,'L');
			//puts("zzzzzzzzzzzzzzzzzzzzzz");
			break;
		case 1:		
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Two-Level Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call twoLevelVMSim()
			twoLevelVMSim(procTable,phyMemFrames);                        
			initProcTableEntry(procTable,1);
                        initPhyMem(phyMemFrames,nFrame);
			break;
		case 2:
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Inverted Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call invertedPageVMsim()
			invertedPageVMSim(procTable,phyMemFrames,nFrame);
			initProcTableEntry(procTable,2);
			initPhyMem(phyMemFrames,nFrame);
			break;
		default:
			printf("=============================================================\n");
			printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with FIFO
                        oneLevelVMSim(procTable,phyMemFrames,'F');
                        initProcTableEntry(procTable,0);
                        initPhyMem(phyMemFrames,nFrame);		
			printf("=============================================================\n");
			printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call oneLevelVMSim() with LRU
                        for(i = 0; i < numProcess; i++) {
                                rewind(procTable[i].tracefp);
                        }
			oneLevelVMSim(procTable,phyMemFrames,'L');
                        initProcTableEntry(procTable,0);
                        initPhyMem(phyMemFrames,nFrame);
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Two-Level Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call twoLevelVMSim()	
			for(i = 0; i < numProcess; i++) {
                                rewind(procTable[i].tracefp);
                        }
			twoLevelVMSim(procTable,phyMemFrames);
                        initProcTableEntry(procTable,1);
                        initPhyMem(phyMemFrames,nFrame);
        		for(i = 0; i < numProcess; i++) {
                		rewind(procTable[i].tracefp);
        		}
			// initialize procTable for the simulation
			printf("=============================================================\n");
			printf("The Inverted Page Table Memory Simulation Starts .....\n");
			printf("=============================================================\n");
			// call invertedPageVMsim()
			invertedPageVMSim(procTable,phyMemFrames,nFrame);
                        initPhyMem(phyMemFrames,nFrame);
			break;
	}
	return(0);
}

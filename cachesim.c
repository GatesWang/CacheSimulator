#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include <gmodule.h>
/*
./cachesim 512 direct p0 fifo 8 trace1.txt
 */

int main(int argc, char ** argv) {
	int cacheSize = atoi(argv[1]);
	const char * assoc = argv[2];
	const char * preFetch = argv[3];
	//const char * replace = argv[4];
	int blockSize = atoi(argv[5]);
	FILE * fp = fopen(argv[6],"r");

	char * pc = (char *) malloc(100*sizeof(char));
	char * op = (char *) malloc(100*sizeof(char));
	char * ad = (char *) malloc(100*sizeof(char));

	int C = cacheSize;
	/*get b*/
	int B = blockSize;
	int b = 0;
	while(B!=1){
		B = B>>1;
		b++;
	}
	//printf("bits for offset %d\n", b);
	B = blockSize;

	/*process assoc*/
	int E = 0;
	if(strcmp(assoc, "direct")==0){
		E = 1;
	}
	else if(strcmp(assoc, "assoc")==0){
		//this is for fully associative set
		E=-1;
	}
	else{
		char * tempAssoc = malloc(100 * sizeof(char));
		strcpy(tempAssoc, assoc);
		tempAssoc+=6;
		E = atoi(tempAssoc);
	}

	/*get S*/
	int setSize = E*B;

	int S = 0;
	if(E==-1){
		S = 1;
		E = C/B;
		setSize = E*B;
	}
	else
	{
		S = C/(setSize);
		//printf("num sets %d\n", S);
	}

	/*get s*/
	int s = 0;
	while(S!=1){
		S = S>>1;
		s++;
	}
	//printf("bits for set %d\n\n", s);


	typedef struct block
	{
	   int setNumber;
	   int tag;
	   int priority;
	} block;

	/*get blockOffset*/
	int getBlockOffset(unsigned long number){
		int blockOffset = (number & ((1<<b)-1));
		return blockOffset;
	}

	/*get setNumber */
	int getSetNumber(unsigned long number){
		int setNumber = (number>>b);
		setNumber = (setNumber	& ((1<<s)-1));
		return setNumber;
	}

	/*get tagIdentifier*/
	int getTagNumber(unsigned long number){
		int tag = (number>>(b+s));
		return tag;
	}

	//given a set and tag, return if its in the cache
	int inCache(int setNumber, int tag, int arrayIndex, GPtrArray * array){
		int inCache = 0;
		int arrayIndex2;
		for(arrayIndex2=0; arrayIndex2<arrayIndex; arrayIndex2++){
			block * currentBlock = g_ptr_array_index(array,arrayIndex2);
			int setNumber2 = currentBlock->setNumber;
			int tag2 = currentBlock->tag;
			if(setNumber2 == setNumber)
			{
				if(tag2 == tag)
				{
					inCache = 1;
					break;
				}
			}
		}
		return inCache;
	}

	//get max priority in the set
	int getMaxPriority(int setNumber, int tag, int arrayIndex, GPtrArray * array){
		int maxPriority = 0;
		int arrayIndex2;
		for(arrayIndex2=0; arrayIndex2<arrayIndex; arrayIndex2++){
			block * currentBlock = g_ptr_array_index(array,arrayIndex2);
			int setNumber2 = currentBlock->setNumber;
			int priority2 = currentBlock->priority;
			if(setNumber2 == setNumber)
			{
				if(priority2 > maxPriority){
					maxPriority = priority2;
				}
			}
		}
		return maxPriority;
	}

	//remove lowest priority
	void removeLowestPriority(int setNumber, int tag, int * arrayIndex, GPtrArray * array){
		int arrayIndex2;
		for(arrayIndex2=0; arrayIndex2<*arrayIndex; arrayIndex2++){
			block * currentBlock = g_ptr_array_index(array,arrayIndex2);
			int setNumber2 = currentBlock->setNumber;
			int priority2 = currentBlock->priority;
			if(setNumber2 == setNumber)
			{
				if(priority2 == 1)
				{
					g_ptr_array_remove(array, currentBlock);
					*arrayIndex = *arrayIndex - 1;
					for(arrayIndex2=0; arrayIndex2<*arrayIndex; arrayIndex2++){
						block * currentBlock = g_ptr_array_index(array,arrayIndex2);
						int setNumber2 = currentBlock->setNumber;
						if(setNumber2 == setNumber){
							currentBlock->priority = currentBlock->priority - 1 ;
						}
					}
					break;
				}
			}
		}
	}

	int memoryReads = 0;
	int memoryWrites = 0;
	int cacheHits = 0;
	int cacheMisses = 0;

	GPtrArray *array = NULL;
	unsigned long number = 0;
	int arrayIndex = 0;
	while(1){
		fscanf(fp, "%s ", pc);
		if(strcmp(pc,"#eof")==0){
			break;
		}
		fscanf(fp, "%s ", op);
		fscanf(fp, "%s ", ad);
		fscanf(fp, "\n");
		number = (unsigned long)strtol(ad, NULL, 0);

		int tag = getTagNumber(number);
		int setNumber = getSetNumber(number);

		if(array == NULL)
		{
			arrayIndex = 0;
			array = g_ptr_array_new();
		}

		if(inCache(setNumber, tag, arrayIndex, array))
		{
			cacheHits++;
			if(strcmp(op,"W")==0)
			{
				memoryWrites++;
			}
		}
		else
		{
			cacheMisses++;
			block * newBlock = malloc(sizeof(block));
			newBlock->setNumber = setNumber;
			newBlock->tag = tag;

			int tagPriority = 0;
			int maxPriority = getMaxPriority(setNumber, tag, arrayIndex, array);
			tagPriority = maxPriority + 1;

			//evict
			if(tagPriority==E+1)
			{
				removeLowestPriority(setNumber, tag, &arrayIndex, array);
				tagPriority = maxPriority;
			}

			newBlock->priority = tagPriority;
			g_ptr_array_add(array, newBlock);
			arrayIndex++;

			/*
			for(arrayIndex2=0; arrayIndex2<arrayIndex; arrayIndex2++){
				block * currentBlock = g_ptr_array_index(array,arrayIndex2);
				int tag2 = currentBlock->tag;
				printf("%X ", tag2);
			}
			printf("\n\n\n");
			*/
			if(strcmp(op,"R")==0)
			{
				memoryReads++;
			}
			else
			{
				memoryReads++;
				memoryWrites++;
			}
				/*prefetching step*/
				if(strcmp(preFetch,"p1")==0)
				{
					number = number + B;
					tag = getTagNumber(number);
					setNumber = getSetNumber(number);

					if(!inCache(setNumber, tag, arrayIndex, array))
					{
						memoryReads++;

						block * newBlock = malloc(sizeof(block));
						newBlock->setNumber = setNumber;
						newBlock->tag = tag;

						int tagPriority = 0;
						int maxPriority = getMaxPriority(setNumber, tag, arrayIndex, array);
						tagPriority = maxPriority + 1;

						//evict
						if(tagPriority==E+1)
						{
							removeLowestPriority(setNumber, tag, &arrayIndex, array);
							tagPriority = maxPriority;
						}

						newBlock->priority = tagPriority;
						g_ptr_array_add(array, newBlock);
						arrayIndex++;
					}
				}//end if prefetch

		}//end if cache miss

	}//end of while loop
	printf("Memory reads: %d\n", memoryReads);
	printf("Memory writes: %d\n", memoryWrites);
	printf("Cache hits: %d\n", cacheHits);
	printf("Cache misses: %d\n", cacheMisses);

	return 0;

}

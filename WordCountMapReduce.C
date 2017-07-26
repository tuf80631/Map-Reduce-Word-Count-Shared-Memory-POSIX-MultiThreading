#include <iostream>  
 #include <cstdlib>  
 #include <pthread.h>  
 #include <stdio.h>  
 #include <fstream>   
 #include <map>  
 #include <unistd.h>  
 #include <sys/mman.h>  
 #include <fcntl.h>  
 #include <sys/stat.h>   
 #include <semaphore.h>  
 using namespace std;  
 #define NUM_THREADS   4  
 std::map<std::string, int> entireMap; //heap  
 std::string charContainer; //heap  
  char *data;  
 int fd, pagesize;  
 struct thread_data{  
   int thread_id; //stack  
   std::map<std::string, int> m ; //stack  
   char* data; //heap?  
   char* tempWord; //heap?  
 } threadData;  
 void *mapper(void *threadarg)  
 {  
   struct thread_data *my_data;  
   my_data = (struct thread_data *) threadarg;  
   int i = 0;  
   int k = 0;  
   while (my_data->data[i]!='\0')  
   {  
     if (my_data->data[i] == ' ' || my_data->data[i] == '\0')   
     {  
         my_data->tempWord[k]='\0';  
         my_data->m[my_data->tempWord]++;  
         k = -1;  
     }  
     else  
       {  
         my_data->tempWord[k]=my_data->data[i];  
       }  
     i++;k++;  
   }  
   //last word since while loop above will finish before  
   if (my_data->data[i]=='\0')  
   {  
     my_data->tempWord[k]='\0';  
     my_data->m[my_data->tempWord]++;  
   }  
   pthread_exit(NULL);  
 }  
 bool hitSpaceAndIndicatesEndOfWord(char& currentCharInContainer, char& previousIndexCharInContainer)  
 {  
   return ((currentCharInContainer == ' ' || currentCharInContainer == '\0' || currentCharInContainer == '\n' ||  
           currentCharInContainer == '\r') && (previousIndexCharInContainer != ' ' &&   
           previousIndexCharInContainer != '\0' && previousIndexCharInContainer != '\n' &&  
           previousIndexCharInContainer != '\r') ) ;  
 }  
 int main ()  
 {  
   clock_t start, end;  
   double msecs;  
   start = clock();  
   //allocate shared memory for data buffer  
   data = (char*)mmap(0, 10000000, PROT_READ|PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, fd,  
   pagesize);  
   //allocated shared memory for semaphore to be shared between child and parent  
   sem_t *sema = (sem_t*)mmap(NULL, sizeof(sema), PROT_READ |PROT_WRITE,MAP_SHARED|MAP_ANONYMOUS,-1, 0);  
   sem_init(sema, 1, 0); //initialize semaphore counter to 0.   
   int pid = fork(); //fork  
   if (pid<0){  
     ;  
   }  
   else if (pid>0) //parent process opens file and reads into shared memory  
   {  
     /*  
     std::ifstream in("test.txt");  
     std::string contents((std::istreambuf_iterator<char>(in)),   
     std::istreambuf_iterator<char>());  
     charContainer = const_cast<char *>(contents.c_str());  
     snprintf (data, 10000000, charContainer.c_str());  
     */      
     FILE *fp = fopen("test.txt", "r");  
     if (fp != NULL) {  
       size_t newLen = fread(data, sizeof(char), 10000000, fp);  
       if ( ferror( fp ) != 0 ) {  
         fputs("Error reading file", stderr);  
       } else {  
         data[newLen++] = '\0';   
       }  
       fclose(fp);  
     }  
     sem_post(sema); //parent unblocks semaphore   
   }  
   else //child process accesses shared memory and performs map reduce.  
   {  
     sem_wait(sema); //child waits  
     int i = 0;  
     cout<<"child"<<endl;  
     std::string charContainer(data);  
     pthread_t threads[NUM_THREADS];  
     struct thread_data td[NUM_THREADS];  
     int rc;  
     int numWords = 1;  
     while (charContainer[i]!='\0')  
     {  
       if (hitSpaceAndIndicatesEndOfWord(charContainer[i],charContainer[i-1]))   
         {  
           numWords++;  
         }  
       i++;  
     }  
     int initialAllocation = numWords/4;  
     int remainder = numWords - numWords/4*4;   
     int firstThreadNumWords = initialAllocation;  
     int secondThreadNumWords = initialAllocation;  
     int thirdThreadNumWords = initialAllocation;  
     int fourthThreadNumWords = initialAllocation;  
     i = 0;  
     while (i<remainder)  
     {  
       firstThreadNumWords++; i++;  
       if(i==remainder)break;  
       secondThreadNumWords++; i++;  
       if(i==remainder)break;  
       thirdThreadNumWords++; i++;  
       if(i==remainder)break;  
       fourthThreadNumWords++; i++;  
       if(i==remainder)break;  
     }  
     i = 0;  
     int k = 0;  
     /** Segregate entire char array into each thread*/  
     for( i=0; i < NUM_THREADS; i++ ){  
       td[i].thread_id = i;  
       td[i].data = new char[1000000];  
       td[i].tempWord = new char[24];  
       //td[i].m = new map <std::string, int> ();  
       int threadIdIndex = 0;  
       int threadWordNum=0;  
       int threadMax = 0;  
       if (i==0) threadMax = firstThreadNumWords;  
       else if (i==1) threadMax = secondThreadNumWords;  
       else if (i==2) threadMax = thirdThreadNumWords;  
       else if (i==3) threadMax = fourthThreadNumWords;  
       while (charContainer[k]!='\0'&&threadWordNum != threadMax)  
       {  
         //boolean to find if character position indicates end of word  
         if (hitSpaceAndIndicatesEndOfWord(charContainer[k], charContainer[k-1]))   
           {  
             threadWordNum++;  
             if (charContainer[k] != '\0') td[i].data[threadIdIndex]=' ';  
           }  
         else if (charContainer[k]!=' ' && charContainer[k]!= '\n' && charContainer[k]!= '\r')  
           {  
             td[i].data[threadIdIndex]=tolower(charContainer[k]);   
           }  
         else  
           {  
            threadIdIndex--;  
           }  
         k++; threadIdIndex++;  
       }  
       td[i].data[threadIdIndex+1]='\0';    
     }  
     //create threads and initiate mapper  
     for( i=0; i < NUM_THREADS; i++ ){  
       rc = pthread_create(&threads[i], NULL,  
                mapper, (void *)&td[i]);  
     }  
     //** wait for all threads to finish before merging  
     for (i = 0; i < NUM_THREADS; i++)  
     {  
       int returnValue;  
       pthread_join(threads[i], (void **)&returnValue);  
     }  
     //reducer. Combines maps of each thread into an overall map that is in entire process  
     for (i=0; i < NUM_THREADS;i++)  
     {  
       map<std::string, int>::iterator p;  
       std::map<std::string,int>::iterator find;  
       int currentValueInMap;  
       int currentValueInThreadMap;  
       int newValueForMap;  
       for(p = td[i].m.begin(); p != td[i].m.end(); p++) {  
         currentValueInThreadMap = p->second;  
         find = entireMap.find(p->first);  
         if (find != entireMap.end())  
         {  
          currentValueInMap = find->second;  
          newValueForMap = currentValueInMap+currentValueInThreadMap;  
          find->second = newValueForMap;  
         }  
         else   
         {  
           entireMap.insert ( std::pair<std::string,int>(p->first,p->second) );   
         }  
       }  
     }  
     map<std::string, int>::iterator p;  
     for(p = entireMap.begin(); p != entireMap.end(); p++) {  
       cout << p->first << " " <<p->second<< endl;  
      }  
     end = clock();  
     msecs = ((double) (end - start)) * 1000 / CLOCKS_PER_SEC;  
     cout<<msecs<<endl;  
     return 0;  
   }  
 }  
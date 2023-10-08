#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/sysmacros.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <sys/mman.h>


#define S_IFMT_ 0xf000
#define S_IFBLK_ 0x6000
#define S_IFCHR_ 0x2000
#define S_IFDIR_ 0x4000
#define S_IFIFO_ 0x1000
#define S_IFLNK_ 0xA000
#define S_IFREG_ 0x8000
#define S_IFSOCK_ 0xC000
/*add underscore*/

/*compile with: gcc -std=gnu99*/

/*
-----------------------------------------------------------------------------------------------------
CHILD
----------------------------------------------------------------------------------------------------
The child process should read a file name from the keyboard and print the files
information gathered with stat. If no file was found with that name, report this back
to the user and wait for the next user input.

Entering “list” lists the content of the current directory (wherever the executable is
being called in. Check: opendir, readdir and closedir).

Entering “q” ends the program (all processes).

“..” should go one level back.

“/foldername” should go into the folder. Report if the folder does not exists.

Report the current working folder to your scanf line:
    stat prog ./CURRENTFOLDER$
    Use getcwd()
-----------------------------------------------------------------------------------------------------
PARENT  
-----------------------------------------------------------------------------------------------------
The parent should check every 10 seconds, if the child was active. If not, kill (brutally) the child
and then end the parent program.

Prevent both process to be killed or terminated.
Right after the “kill” function in the parent, make sure to wait or you end up having a zombie
-----------------------------------------------------------------------------------------------------

*/

struct stat sb;

/*functions*/
void cyan(); /*printing in different color*/
void default_color();
void print_file_stats(struct stat sb);
void print_cur_dir(int *active);
int check_file(char *file, int *active);
void signal_handler(int signal);

/*
swap roles of child and parent
run on server: result = infinite loop there too

Issue: when parent does the work, no infinite loop, but when child does the wirk, infinite
*/

int main(int argc, char *argv[]){
    int pid, child_status, i, *active;
    char user_input[1000];
    char *folder;
    size_t len;
    ssize_t length;
    struct stat sb;
    char new_path[1000];
    char path[1000];
    len = 0;

    active = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED|0x20, -1, 0);

    active[0] = 1;

    pid = fork();

    signal(15, signal_handler);
    signal(3, signal_handler);
    signal(2, signal_handler);

    if (pid == -1){
        munmap(active, sizeof(int));
        perror("Error in fork(). Exiting\n");
        exit(EXIT_FAILURE);
    }

    if (pid == 0){ 
        /*child*/
        getcwd(path,1000);
        cyan();
        fprintf(stderr,"stat prog%s", path);
        default_color();
        fprintf(stderr,"$ ");
        fflush(stderr);

        fgets(user_input, sizeof(user_input), stdin); 
        user_input[strcspn(user_input, "\n")] = '\0';
        active[0] = 1;

        fflush(stdout);
        

        while(strcmp(user_input, "q") != 0){
            /*fprintf(stderr, "\nActive in child: %d\n", active[0]);*/

            if (strcmp(user_input, "list") == 0){
                print_cur_dir(active);
            }

            else if (strcmp(user_input, "..") == 0){
                /*change directory to parent*/
                if(chdir("..") != 0){
                    munmap(active, sizeof(int));
                    perror("Error changing directory to parent directory. Exiting.\n");
                    exit(EXIT_FAILURE);
                }
                getcwd(path,1000);
            }

            else if (user_input[0] == '/' || (user_input[0] == '.' && user_input[1] == '/')){
                /*go into child folder*/
                /*move pointer based on input (/ or ./)*/
                if (user_input[0] == '/'){
                    folder = user_input + 1;
                }
                else{
                    folder = user_input + 2;
                }
                
                if(check_file(folder, active)== 0){
                    fprintf(stderr,"Folder not found;\n");
                }
                else{
                    /*get new path*/
                    if (realpath(folder, new_path) == NULL){
                        munmap(active, sizeof(int));
                        perror("Error in realpath(). Exiting\n");
                        exit(EXIT_FAILURE);
                    }
                    /*set dir to new path*/
                    /*fix this to continue to run*/
                    if(chdir(new_path) != 0){
                        /*keep going? or terminate*/
                        /*ex: ./hi.txt*/
                        perror("Not a valid folder.\n");
                        
                    }
                    getcwd(path,1000);
                }
            }
            else{
                /*check file name*/
                if(check_file(user_input, active) == 0){
                    fprintf(stderr,"Invalid input.\n");
                }
                else{
                    if (stat(user_input, &sb) != 0){
                        munmap(active, sizeof(int));
                        perror("Error in stat(). Exiting.\n");
                        exit(EXIT_FAILURE);
                    }
                    print_file_stats(sb);
                }
            }
            /*where inactivity occurs: terminate child after 10 sec*/

            /*user input*/
            cyan();
            getcwd(path, 1000);
            fprintf(stderr,"stat prog%s", path);
            default_color();
            fprintf(stderr,"$ ");
            fflush(stderr);

            fgets(user_input, sizeof(user_input), stdin);
            user_input[strcspn(user_input, "\n")] = '\0'; 

            active[0] = 1;

            fflush(stdout);            
        }
        active[0] = 0;
        kill(getppid(), SIGKILL);
        kill(pid, SIGKILL);
    }

    else{
        /*parent*/
        /*
        while
        sleep 10
        check
        shared mem: int active flag
        if active == 1
            active = 0 
        */
        while (1){
            sleep(10);
            
            if (active[0] == 0){
                fprintf(stderr, "\nTerminating child process due to inactivity.\nEnding program.\n");
                kill(pid, SIGKILL);
                wait(0);
                break;
            }
            else {
                active[0] = 0;
            }
        }
    }  
    /*kill(pid, SIGKILL);*/
    return 0;
}

void cyan(){
    fprintf(stderr,"\033[0;36m");
}

void default_color(){
    fprintf(stderr,"\033[0m");
}

/*taken from prof humer's example code*/
void print_file_stats(struct stat sb){
     fprintf(stderr,"----------------------------------------------------------\n");
     fprintf(stderr,"ID of containing device:  [%jx,%jx]\n", 
            (uintmax_t) major(sb.st_dev), 
            (uintmax_t) minor(sb.st_dev));

    fprintf(stderr,"File type:                ");

    switch (sb.st_mode & S_IFMT_) {
    case S_IFBLK_:  fprintf(stderr,"block device\n");            break;
    case S_IFCHR_:  fprintf(stderr,"character device\n");        break;
    case S_IFDIR_:  fprintf(stderr,"directory\n");               break;
    case S_IFIFO_:  fprintf(stderr,"FIFO/pipe\n");               break;
    case S_IFLNK_:  fprintf(stderr,"symlink\n");                 break;
    case S_IFREG_:  fprintf(stderr,"regular file\n");            break;
    case S_IFSOCK_: fprintf(stderr,"socket\n");                  break;
    default:       fprintf(stderr,"unknown?\n");                break;
    }

    fprintf(stderr,"I-node number:            %ju\n", (uintmax_t) sb.st_ino);

    fprintf(stderr,"Mode:                     %jo (octal)\n",
            (uintmax_t) sb.st_mode);

    fprintf(stderr,"Link count:               %ju\n", (uintmax_t) sb.st_nlink);
    fprintf(stderr,"Ownership:                UID=%ju   GID=%ju\n",
            (uintmax_t) sb.st_uid, (uintmax_t) sb.st_gid);

    fprintf(stderr,"Preferred I/O block size: %jd bytes\n",
            (intmax_t) sb.st_blksize);
    fprintf(stderr,"File size:                %jd bytes\n",
            (intmax_t) sb.st_size);
    fprintf(stderr,"Blocks allocated:         %jd\n",
            (intmax_t) sb.st_blocks);

    fprintf(stderr,"Last status change:       %s", ctime(&sb.st_ctime));
    fprintf(stderr,"Last file access:         %s", ctime(&sb.st_atime));
    fprintf(stderr,"Last file modification:   %s", ctime(&sb.st_atime));
    fprintf(stderr,"----------------------------------------------------------\n");

}

void print_cur_dir(int *active){
    struct dirent *entry;
    DIR *dir;

    dir = opendir("."); /*open current directory*/
    if (dir == NULL){
        munmap(active, sizeof(int));
        perror("File does not exist. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    /*fprintf(stderr,"\nChild PID: %d\n", getpid()); remove before submission*/

    /*print directory every 10 seconds*/
    while((entry = readdir(dir)) != NULL){
        fprintf(stderr,"File: %s\n", entry->d_name);
    }

    closedir(dir);
}

int check_file(char *file, int *active){
    struct dirent *entry;
    DIR *dir;

    dir = opendir("."); /*open current directory*/
    if (dir == NULL){
        closedir(dir);
        munmap(active, sizeof(int));
        perror("File does not exist. Exiting.\n");
        exit(EXIT_FAILURE);
    }

    /*read directory, find if there*/
    while((entry = readdir(dir)) != NULL){
        if(strcmp(entry->d_name, file) == 0){
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

void signal_handler(int signal){
   /*fprintf(stderr,"Preventing termination\n");*/
}
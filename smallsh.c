/*Name: Jason Watson
 * CS 344 Program 3 Smallsh
 * 5/26/2019
 * This program writes a shell in C.  The shell runs basic command line instructions and will return the results, similar
 * to what the bash shell will do.  When the program starts up, there is a comand line prompt where the user can enter basic
 * commands.  This shell supports 3 build-in commands: exit, cd and status.  It also supports comment with the # character.
 * These symbols are recognized: <, > and &, but they must be surrounded by spaces.  When parsing, these spaces need to be 
 * removed..  If the last character is &, then the command must be executed in the background.  Standard in and out can also be
 * redirected with the < and > words followed by a filename word.  Input redirection can appear before or after output redirection.
 * The shell also supports command lines with a maximum length of 2048 and with a maximum of 512 arguments.  
 * This program will need to make use of fork(), exec() and waitpid() to execute commands.
*/



#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

/*Use at least C99, or later, which allows for a boolean library header*/
#include <stdbool.h>

/*You can potentially build structs in place of globals and will do that if I have time*/

/*Globals *************************************/

int exitForegroundStatus;
bool isForeground = false;
bool isBackground;
char iFile[200];
char oFile[200];
int numberArguments;
char *argumentHolder[512];
char commandLine[2048];
char oFile[200];
char iFile[200];
int backgroundPidNumber;
pid_t backgroundPidHolder[512];



/*Globals ************************************/

int changeDirectory(char* iBuffer);
bool checkLeadingChar(char *str);
void parseCommandLine(char* iBuffer);
void initializeArgumentList(char** args);
void reDirects();
void runCommand();



/*Signal handler functions*/
void StopSignalTrap(int sig);
void ChildSignalTrap(int sig);
void TermSignalTrap(int sig);


int main() {
    //First, initialize background pid variables and array with bogus -1 values.
    int a;
    backgroundPidNumber = -1;
    for (a = 0; a < 512; a++)
    {
        backgroundPidHolder[a] = -1;
    }

    /*Have a vessel to store the user input*/
    char iBuffer[2048];
    /*This will be where I will hold the status of an exited process*/
    int foregroundStatus;
    char tempBuffer[2048];
    char *iFileName;
    char *oFileName;
    char *temp;

    struct sigaction stopSignal = {0};
    struct sigaction termSignal = {0};
    struct sigaction childSignal = {0};

    /*initialize my signal handlers*/
    stopSignal.sa_handler = StopSignalTrap;
    sigfillset(&stopSignal.sa_mask);
    stopSignal.sa_flags = 0;


    termSignal.sa_handler = TermSignalTrap;
    sigfillset(&termSignal.sa_mask);
    termSignal.sa_flags = 0;

    childSignal.sa_handler = ChildSignalTrap;
    sigfillset(&childSignal.sa_mask);
    childSignal.sa_flags = 0;


    /*Now, begin the program itself.  Use an eternal do/while loop.*/
    do
    {
        /*All of the signal handlers must be reset with each iteration of this loop*/

        sigaction(SIGTSTP, &stopSignal, NULL);
        sigaction(SIGINT, &termSignal, NULL);
        sigaction(SIGCHLD, &childSignal, NULL);

        /*Now, I need to switch the foreground global depending on whether or not program returns a terminate signal.*/
        /*Get the terminating signal with the WTERMSIG macro, as from the lectures*/
        /*This will likewise need to be done with each iteration of the do/while loop*/
        if (WTERMSIG(exitForegroundStatus) == 11 && isForeground == true)
        {
            printf("\nExiting foreground-only mode\n");
            isForeground = false;
        }
        else if (WTERMSIG(exitForegroundStatus) == 11 && isForeground == false)
        {
            printf("\nEntering foreground-only mode (& is now ignored)\n");
            isForeground = true;
        }

        // flush stdin and stdout
        fflush(stdout);
        fflush(stdin);

        /*per the assignment description, this will be the command prompt*/
        printf(": ");
        /*set up iBuffer*/
        memset(iBuffer, '\0', sizeof(iBuffer));
        /*get the user input and store it into the input buffer*/
        fgets(iBuffer, sizeof(iBuffer), stdin);
        /*convert $$ to pid*/
        /*This should ideally expand any "$$" to pid*/
        char pidString[20];
        memset(pidString, '\0', sizeof(pidString));
        /*output pid Number to pidString.  Use sprintf() function here*/
        sprintf(pidString, "%d", getpid());
        /*parse line.  This is taking some trial and error here, but I think I got it to work correctly.*/
        /*Use strstr() function to get first occurrence of "$$". I need to expand "$$" to the pid number*/
        while (strstr(iBuffer, "$$") != NULL)
        {
            char *foundIt = strstr(iBuffer, "$$");

            strcpy(foundIt + strlen(pidString), foundIt + 2);
            strncpy(foundIt, pidString, strlen(pidString));
        }

        // flush stdin and stdout again
        fflush(stdout);
        fflush(stdin);

        /*First built-in command will be exit.  I will need to cause the program to exit gracefully*/
        /*first, try to use strcmp*/
        if (strncmp(iBuffer, "exit", 4) == 0)
        {
            int a;
            /*kill off each individual background process*/
            for (a = 0; a < backgroundPidNumber + 1; a++)
            {
                // You will need to interrupt all background processes
                kill(backgroundPidHolder[a], SIGINT);
            }
            /*Then, exit out of the parent process*/
            exit(0);
        }
            /*built-in command "cd", to change the directory*/
        else if (strncmp(iBuffer, "cd", 2) == 0)
        {
            /*Call the changeDirectory function here, and pass into it the input buffer*/
            changeDirectory(iBuffer);

        }
            /*built-in command "status", to check the status of the previous foreground command*/
        else if (strncmp(iBuffer, "status", 6) == 0)
        {
            // check how foreground exited.  foregroundStatus will be either 0, for exited normally, or 1, for error*/
            if (WEXITSTATUS(exitForegroundStatus))
            {
                foregroundStatus = WEXITSTATUS(exitForegroundStatus);
            }
                /*else if terminated by a signal, I will need the signal number*/
            else
            {
                foregroundStatus = WTERMSIG(exitForegroundStatus);
            }
            /*print out the foreground status*/
            printf("exit value %d\n", foregroundStatus);
        }
            /*Next build-in command will be the comment command "#"*/
        else if (strncmp(iBuffer, "#", 1) == 0)
        {
            /*I'll just have the program continue*/
            continue;
        }
            /*Otherwise, it will be a non-built-in command, which I will need to parse and then run the command itself*/
        else
        {
            if (iBuffer != NULL && strcmp(iBuffer, "") != 0)
            {


                // If none of the above, then read in a command
                //You will need to parse the command
                //Initialize global numArguments variable
                /*I could refer to what I did in program 2 for help with this.*/
                numberArguments = 0;
                iBuffer[strlen(iBuffer) -1] = '\0';

                /*initialize iFile and oFile globals*/

                memset(iFile, '\0', sizeof(iFile));
                memset(oFile, '\0', sizeof(oFile));

                /*initialize the commandLine global*/
                memset(commandLine, '\0', sizeof(commandLine));

                /*is it a background process?*/
                if (iBuffer[strlen(iBuffer)-1] == '&')
                {
                    isBackground = true;
                    iBuffer[strlen(iBuffer)-1] = '\0';
                }
                else
                {
		    /*not a background process*/
                    isBackground = false;
                }


                memset(tempBuffer, '\0', sizeof(tempBuffer));
                /*Copy iBuffer into tempBuffer*/
                strcpy(tempBuffer, iBuffer);
                /*You must remove the empty space from tempBuffer string*/
		/*use strtok() function, which I also used in program 2, if I need to remember how*/
                strtok(tempBuffer, " ");
                /*copy tempBuffer to the commandLine array*/
                strcpy(commandLine, tempBuffer);

                memset(tempBuffer, '\0', sizeof(tempBuffer));
                strcpy(tempBuffer, iBuffer);
                /*use strstr() to find the first occurrence of the substring "<" in tempBuffer, then store in iFileName*/
                /*if there is no "<" in substring, then return null pointer*/
                iFileName = strstr(tempBuffer, "<");
                if (iFileName != NULL)
                {
                    /*use memcpy to copy from one memory location to another.  This is to copy everything after the "<"*/
                    memcpy(iFileName, iFileName+2, strlen(iFileName));
                    /*get rid of the extra space*/
                    strtok(iFileName, " ");
                    /*tack on a '\0', in order to make the input a string*/
                    iFileName[strlen(iFileName)] = '\0';
                    /*copy to global iFile*/
                    strcpy(iFile, iFileName);
                }
                /*repeat as above but for stdout*/
                memset(tempBuffer, '\0', sizeof(tempBuffer));
                strcpy(tempBuffer, iBuffer);
                oFileName = strstr(tempBuffer, ">");
                if (oFileName != NULL)
                {
                    memcpy(oFileName, oFileName+2, strlen(oFileName));
                    strtok(oFileName, " ");
                    oFileName[strlen(oFileName)] = '\0';
                    strcpy(oFile, oFileName);
                }

                /*finally, prepare tempBuffer for rest of command, i.e. arguments*/
                memset(tempBuffer, '\0', sizeof(tempBuffer));
                strcpy(tempBuffer, iBuffer);
                /*remove space from command*/
                strtok(tempBuffer, " ");

                temp = strtok(NULL, "");


                if (checkLeadingChar(temp) == false)
                {
                    strcpy(tempBuffer, temp);
		    /*remove characters that you don't need*/
                    strtok(tempBuffer, "<>&#");
                    /*again, remove space from command*/
                    strtok(tempBuffer, " ");
                    /*Here, assign the first argument to global argument holder*/
                    argumentHolder[0] = tempBuffer;
                    /*There is now 1 argument*/
                    numberArguments = 1;
                    temp = strtok(NULL, " ");
                    while (temp != NULL)
                    {
                        /*Here, increment the number of arguments*/
                        argumentHolder[numberArguments] = temp;
                        numberArguments++;
                        temp = strtok(NULL, " ");
                    }
                    argumentHolder[numberArguments] = strtok(NULL, "");
                }
                //You will then need to run the command itself.
                runCommand();
                //Then you will  need to clean up the data used and re-initialize isBackground to false
                isBackground = false;
                memset(iFile, '\0', sizeof(iFile));
                memset(oFile, '\0', sizeof(oFile));
                memset(commandLine, '\0', sizeof(commandLine));
            }
            else
            {
                continue;
            }
        }
    }while(true);

    return 0;
}

/*changeDirectory() function:  This function will be called from main() when the user enters  build-in "cd"
* command.  It will return an integer that will be either a 0 (for success) or 1 (for failure).
 * It will examine the command line for the cd command, and either send the user to the home directory or to path, if I have time*/
int changeDirectory(char* iBuffer)
{
    /*Assign the home environmental variable to homeDirectoryPath*/
    char* homeDirectoryPath = getenv("HOME");
    char newPath[2048];

    iBuffer[strlen(iBuffer) -1] = '\0';

    if (strcmp(iBuffer, "cd") == 0)
    {
        if (chdir(homeDirectoryPath) != 0)
        {
            printf("no such file or directory");
            return 1;
        }
        return 0;
    }

    memset(newPath, '\0', sizeof(newPath));

    /*remove any spaces from command line*/
    strtok(iBuffer," ");
    strcpy(iBuffer, strtok(NULL,""));
    /*If first element of command is "/", then...*/
    if (iBuffer[0] == '/')
    {
        /*Assign homeDirectoryPath and iBuffer to newPath*/
        sprintf(newPath, "%s%s", homeDirectoryPath, iBuffer);
    }
    else
    {
        /*Else, assign iBuffer to newPath*/
        sprintf(newPath, "%s", iBuffer);
    }

    /*change directory*/
    if (chdir(newPath) != 0)
    {
        printf("no such file or directory");
        return 1;
    }
    return 0;

}

/*checkLeadingChar() function: This is a helper function, called while parsing the command line for excess arguments 
 * This function will need to return either a true or false*/
bool checkLeadingChar(char *str)
{

    if (str == NULL)
    {
        return true;
    }
    /*If iBuffer begins with '&'*/
    if (str[0] == '&')
    {
        return true;
    }
    /*or if it begins with '#'*/
    else if (str[0] == '#')
    {
        return true;
    }
    /*or if it begins with '<'*/
    else if (str[0] == '<')
    {
        return true;
    }
    /*or if it begins with '>'*/
    else if (str[0] == '>')
    {
        return true;
    }
    /*else, return false*/
    else
    {
        return false;
    }
}


/*InitializeArgumentList() function:  This function prepares the argument list for execvp*/
void initializeArgumentList(char** args)
{
    int a;
    /*assign first argument*/
    args[0] = commandLine;
    for (a = 0; a < numberArguments; a++)
    {
        if (argumentHolder[a] != NULL)
        {
   	    /*add remaining arguments to command*/
            args[a + 1] = argumentHolder[a];
        }
    }
}

/*reDirects() function: This function will set up the redirection of input and output file descriptors. This must be done
 * before execvp function call!*/
/*"https://www.geeksforgeeks.org/input-output-system-calls-c-create-open-close-read-write/" and lecture 3.4 greatly helped me here*/
/* Also: https://stackoverflow.com/questions/15102992/what-is-the-difference-between-stdin-and-stdin-fileno*/
void reDirects()
{

    /*hold the input file descriptor*/
    int ifd = 0;

    if (iFile[0] != '\0')
    {
        ifd = open(iFile, O_RDONLY);

        if (ifd < 0)
        {
            printf("File not found\n");
            exit(1);
        }
        else
        {
            /*From the lecture, I will call dup2() to redirection stdin 0 to point to where input file descriptor points*/
            int result = dup2(ifd, 0);
            if (result == -1)
            {
                perror("dup2");
                exit(2);
            }
            close(ifd);
        }
    }
    /*do the same as above with output file descriptor 1*/
    int ofd = 1;
    if (oFile[0] != '\0')
    {
        /*open file for write only, create file if it doesn't exist, and give privileges*/
        ofd = open(oFile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (ofd < 0)
        {
            printf("file not found\n");
            exit(1);
        }
        else
        {
            /*same as above except redirect stdout 1 to oFile*/
            int result = dup2(ofd, 1);

            if (result == -1)
            {
                perror("dup2");
                exit(2);
            }
            close(ofd);
        }
    }

}

/*runCommand() function:  This function will use the fork() function to create a child for a command.
 * Use the basic structure of the fork() switch statement from the lecture. */
void runCommand()
{
    pid_t spawnpid = -5;
    spawnpid = fork();
    /*I need to hold the command arguments*/
    char *argumentList[512];
    /*Keep track of how the process ends*/
    int childExitStatus = -5;;

    switch(spawnpid)
    {
        case -1:
            printf("Something went wrong\n");
            exit(1);
            break;
        case 0:
            /*prepare redirection for input and output*/
            reDirects();

            /*prepare the arguments for execvp.  Use argumentList[]*/
            initializeArgumentList(argumentList);

            execvp(commandLine, argumentList);

            printf("command not found\n");
            /*please be sure to exit!*/
            exit(1);
            break;
        default:
            if (isBackground == true && isForeground == false)
            {
		/*flush stdout*/
                fflush(stdout);
                printf("background Pid is %d\n", spawnpid);

                /*you will need to add to the background pid holder*/
                backgroundPidHolder[++(backgroundPidNumber)] = spawnpid;

                /*get the background pid #*/
                printf("Background Pid is %d\n", backgroundPidHolder[backgroundPidNumber]);
            }
            else
            {
                /*as from the lecture*/
		/*block the parent until the the child process with the specified pid terminates*/
                waitpid(spawnpid, &childExitStatus, 0);
		/*then, assign the exit status*/
                exitForegroundStatus = childExitStatus;
            }
            break;

    }
}

/*StopSignalTrap() function:  This signal handler catches the ^z interrupt */
void StopSignalTrap(int sig)
{
    if (isForeground == false)
    {
        // Enable foreground only mode
        char* message = ("\nEntering foreground-only mode\n");
        /*Can't use printf here.  Will use write() and STDOUT_FILENO, from the lecture*/
        write(STDOUT_FILENO, message, 35);
        isForeground = true;
    }
    else
    {
	/* Remove foreground mode */
        char* message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 35);
        isForeground = false;
    }

}

/*ChildSignalTrap() function:  This function tracks how a child process ends.*
 * I got help from several sites for this: "stackoverflow.com/questions/31664441/monitor-if-a-process-has-terminated-in-c"
 * "stackoverflow.com/questions/2377811/tracking-the-death-of-a-child-process"
 */
void ChildSignalTrap(int sig)
{
    int a, b;
    pid_t spawnpid = -5;;
    int childExitStatus = -5;;

    for (a = 0; a < backgroundPidNumber + 1; a++)
    {
	/*check if the process specified has completed, return immediately with 0 if it has not completed
 	*this is so background child processes can be cleaned up*/
        spawnpid = waitpid(backgroundPidHolder[a], &childExitStatus, WNOHANG);
        /* If the exit status is either 1 or 0, or the spawnpid does not equal 0*/
	if ((childExitStatus == 0 || childExitStatus == 1) && spawnpid != 0)
        {
	    /*record the background pid# and the exit value of non-signal exit*/
            fprintf(stdout, "\nbackground pid %d is done: exit value %d\n", spawnpid, childExitStatus);
            /*You will need to adjust your global backgroundPid array of completed processes*/
	    int pidPosition = 0;
            for (b = 0; b < backgroundPidNumber + 1; b++)
            {
                  if (backgroundPidHolder[b] == spawnpid)
                  {
                      pidPosition = b;
                      break;
                  }
            }
	    /*Now, once you find the pid#, then you need to make the adjustment*/
            for (b = pidPosition; b < backgroundPidNumber + 1; b++)
            {
                 backgroundPidHolder[b] = backgroundPidHolder[b + 1];
            }
	    /*you can decrement the pid# here*/
            backgroundPidNumber--;
        }
        else if (spawnpid != 0)
        {
            fprintf(stdout, "\nbackground pid %d is done: terminated by signal %d\n", spawnpid, childExitStatus);
	    /*repeat everything that I did above to adjust the global pid variables*/
            int pidPosition = 0;
            for (b = 0; b < backgroundPidNumber + 1; b++)
            {
                if (backgroundPidHolder[b] == spawnpid)
                {
                    pidPosition = b;
                    break;
                }
            }

            for (b = pidPosition; b < backgroundPidNumber + 1; b++)
            {
                backgroundPidHolder[b] = backgroundPidHolder[b + 1];
            }
	    /*again decrement pidnumber*/
            backgroundPidNumber--;

        }
    }

}

// Signal handler for ^c of a foreground process
void TermSignalTrap(int sig)
{
    /*what is the signal number?*/
    printf("\nterminated by signal %d\n", sig);
}


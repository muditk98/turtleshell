/*
* Turtleshell - A simple and easy to use shell. All commands and redirects 
* are same as the ones in bash. Also supports a custom math mode and memory
* tracker.
*/


#include <iostream>                     // cin, cout
#include <stdio.h>                      // System flags
#include <unistd.h>                     // fork() and pthread creation
#include <string>                       // For the strings
#include <cstring>                      // strcat(), strcmp(), etc
#include <vector>						// For the vectors
#include <fstream>						// For file handling
#include <sys/types.h> 					// wait()
#include <pwd.h>						// Working directory
#include <stdlib.h>						// General utility functions
#include <sys/wait.h>					// wait()
#include <fcntl.h>						// dup(),dup2()
#include <math.h>						// For the math
#include <sstream>						// stringstream for string building
#include <pthread.h>					// For the pthreads
#include <time.h>						// Time
#include <iomanip>						// std::quoted

using namespace std;

#define HISTORY_FILE "/.turtlesh_history"

const char *homedir;

bool exitstatus = false;

float MAXMEMLIMIT = 70;

char* expressionToParse;

enum ConcatType {NONE, PIPE, AREDIR, TREDIR, DOUBLEAMP, SEMICOLON}; 

/*GENERAL FUNCTIONS */

std::vector<char*> ReadLine();
std::vector<char*> ReadString(const char*);
int WriteToHistory(std::vector<char*>);
int Execute(std::vector<char*>);
int PipedExecute(std::vector<char*>, std::vector<char*>);
void Print(std::vector<char*>);
void FreeArgs(std::vector<char*>);
int ParsedExecute(std::vector<char*>);
int RedirExecute(std::vector<char*>, std::vector<char*>, ConcatType);
int Run(std::vector<char*>);
void* MonitorMem(void*);


/* MATH RELATED FUNCTIONS*/
float number();
char get();
char peek();
float factor();
float factorial();
float term();
float texp();
float expression();

/* Stopwatch class - A stopwatch to measure time. */
class Stopwatch
{
	clock_t timer;
public:
	Stopwatch(){
		timer = 0;
	}
	void start(){
		timer = clock();
	}
	double stop(){
		return ((clock() - timer)/(double)CLOCKS_PER_SEC);
	}
} globalwatch;

int main()
{
	/* Get home directory */
	if ((homedir = getenv("HOME")) == NULL)
	{
	    homedir = getpwuid(getuid())->pw_dir;
	}
	int status = 0;
	std::vector<char*> argv;
	char prompt[1024];
	pthread_t tid;
	pthread_create(&tid, NULL, MonitorMem, NULL);  // Memory monitoring thread.
	while(exitstatus == false)                     // Main loop - The shell exits when an appropriate
	{                                              // exit signal is given.
		cout << getcwd(prompt, 1024) << "$ ";
		argv = ReadLine();
		status = ParsedExecute(argv);
		WriteToHistory(argv);
		FreeArgs(argv);
		argv.clear();
	}
	return 0;
}

/* Memory monitoring - Every 30s, this function outputs the top 3 processes sorted 
 * by memory usage along with their PIDs. The user can choose to kill any process 
 * that is using too much memory.*/
void* MonitorMem(void* args)
{
	std::vector<char*> argv1 = ReadString("ps -eo pid,ppid,cmd,%mem,%cpu --sort=-%mem | head -4");
	std::vector<char*> argv2 = ReadString("free | grep Mem | awk \"{print $3/$2 * 100.0}\" > tempfile");

	float result;
	ifstream fin;
	char prompt[1024];
	Stopwatch t;
	pid_t childpid;
	while(true)
	{
		ParsedExecute(argv2);
		fin.open("tempfile");
		fin >> result;
		fin.close();
		if(result > MAXMEMLIMIT)
		{
			childpid = fork();
			if(childpid == 0)
			{
				cout << "\nYour memory usage exceeds " << MAXMEMLIMIT << "%. The top 3 processes are: \n";
				ParsedExecute(argv1);
				cout << "Kill the process using the kill command\n";
				cout << getcwd(prompt, 1024) << "$ ";
				exit(EXIT_SUCCESS);				
			}
			else
			{
				waitpid(childpid, NULL, 0);
			}
		}
		t.start();
		while(t.stop() < 30);
	}

}

/* ReadLine() function to read from the standard input. */
vector<char*> ReadLine()
{
	string line;
	string word;
	vector<char*> argv;
	char* s;
	getline(cin, line);
	stringstream sin(line);
	while(sin >> quoted(word))
	{
		s = new char[word.length() + 1];
		strcpy(s, word.c_str());
		argv.push_back(s);
	}
	return argv;
}

/* Takes a constant character string and breaks it up accordingly. */
vector<char*> ReadString(const char* str = NULL)
{
	string word;
	vector<char*> argv;
	stringstream sin(str);
	char* s;
	while(sin >> quoted(word))
	{
		s = new char[word.length() + 1];
		strcpy(s, word.c_str());
		argv.push_back(s);
	}
	return argv;
}

/* This function writes all commands to the history file specified. */
int WriteToHistory(std::vector<char*> argv)
{
	fstream fout;
	string histfile = HISTORY_FILE;
	histfile = homedir + histfile;
	fout.open(histfile.c_str(), ios::out | ios::app);
	if(!fout.is_open())
		return 0;
	for (std::vector<char*>::iterator it = argv.begin(); it != argv.end(); ++it)
	{
		fout << *it << ' ';
	}
	fout << endl;
	fout.close();
	return 1;
}

/* Creates a child process and replaces it with the specified command process. */
int Execute(std::vector<char*> argv)
{
	pid_t childpid;
	int status;

	childpid = fork();

	if (childpid < 0)
	{
		perror("Execute failed to fork");
		exit(EXIT_FAILURE);
	}

	if(childpid == 0)	
	{
		// child process
		argv.push_back(NULL);	// so that it is compliant with execvp
		if( execvp(argv[0], &argv[0]) == -1)
		{
			perror("Execute failed at execvp");
		}
		exit(EXIT_FAILURE);
	}
	else
	{
		// parent process
		waitpid(childpid, &status, 0);
		if(WIFEXITED(status))	
			return WEXITSTATUS(status);
		else
			return EXIT_FAILURE;
	}

}

/* Parses the input obtained from the ReadLine() function and 
 * splits it according to the type of separator found. Then it
 * executes the commands within, recursively if need be. */
int ParsedExecute(std::vector<char*> argv)
{
	ConcatType result = NONE;
	std::vector<char*>::iterator i;
	for (i = argv.begin(); i != argv.end(); ++i)
	{
		if( !strcmp(*i, "|") )
		{
			result = PIPE;
			break;
		}
		else if( !strcmp(*i, ">>") )
		{
			result = AREDIR;
			break;
		}
		else if( !strcmp(*i, ">") )
		{
			result = TREDIR;
			break;
		}
		else if( !strcmp(*i, "&&") )
		{
			result = DOUBLEAMP;
			break;
		}
		else if( !strcmp(*i, ";") )
		{
			result = SEMICOLON;
			break;
		}
	}
	if( i == argv.end() )	// implies result == NONE
	{
		// No pipes. simply execute
		return Run(argv);
	}

	std::vector<char*> arg1(argv.begin(), i);       
	std::vector<char*> arg2(i+1, argv.end());
	if (result == PIPE)
	{
		return PipedExecute(arg1, arg2);
	}
	else if(result == TREDIR || result == AREDIR)
	{
		return RedirExecute(arg1, arg2, result);
	}
	else if (result == DOUBLEAMP)
	{
		return !Run(arg1) && ParsedExecute(arg2);
	}
	else if (result == SEMICOLON)
	{
		Run(arg1);
		return ParsedExecute(arg2);
	}
}

/* Executes arg1 and redirects to the file specified by arg2. */

int RedirExecute(std::vector<char*> arg1, std::vector<char*> arg2, ConcatType result)
{
	int fd[2];
	int fp;
	pipe(fd);
	int status = 1;
	int saved_stdout;
	char c;
	pid_t child1 = fork();

	if (child1 < 0)
	{
		perror("PipedExecute failed to fork");
		exit(EXIT_FAILURE);
	}

	if(child1 == 0)
	{
		// child process
		close(fd[1]);	
		dup2(fd[0], 0);
		if(result == AREDIR)
		{
			fp = open(arg2[0], O_WRONLY | O_CREAT | O_APPEND, 0666);
		}	
		else
		{
			fp = open(arg2[0], O_WRONLY | O_CREAT | O_TRUNC, 0666);
		}
		// open() returns a -1 if an error occurred
		if (fp < 0)
		{
			perror("RedirExecute unable to open file");
			return -1;
		}

		while (read(0, &c, 1) > 0)
			write(fp, &c, 1);

		close(fp);
		// cout << "Exiting arg2";
		exit(EXIT_SUCCESS);
	}
	else
	{
		// cout << "Entering arg1\n";
		close(fd[0]);
		saved_stdout = dup(1);
		dup2(fd[1], 1);
		status = Run(arg1);
		close(fd[1]);
		dup2(saved_stdout, 1);
		close(saved_stdout);
		waitpid(child1, &status, 0);
	}
	return status;
}

/* Executes arg1, and then pipes the output to arg2. */
int PipedExecute(std::vector<char*> arg1, std::vector<char*> arg2)
{
	

	int fd[2];

	pipe(fd);
	int status = 1;
	int saved_stdout;
	pid_t child1 = fork();

	if (child1 < 0)
	{
		perror("PipedExecute failed to fork");
		exit(EXIT_FAILURE);
	}

	if(child1 == 0)
	{
		// child process
		// cout << "Entering arg2\n";
		close(fd[1]);	
		dup2(fd[0], 0);	
		status = ParsedExecute(arg2);
		// cout << "Exiting arg2";
		exit(status);
	}
	else
	{
		// cout << "Entering arg1\n";
		close(fd[0]);
		saved_stdout = dup(1);
		dup2(fd[1], 1);
		status = Run(arg1);
		close(fd[1]);
		dup2(saved_stdout, 1);
		close(saved_stdout);
		waitpid(child1, &status, 0);
	}
	return status;
}

/* Utility function to print a vector. */
void Print(std::vector<char*> argv)
{
	for (std::vector<char*>::iterator it = argv.begin(); it != argv.end(); ++it)
	{
		cout << *it << '\n';
	}
	cout << endl;
}

/* Frees up all the character pointers used by the input argument vector
 * to manage space. */
void FreeArgs(std::vector<char*> argv)
{
	for (std::vector<char*>::iterator i = argv.begin(); i != argv.end(); ++i)
	{
		delete[] *i;
	}
}

/* Looks for shell builtins in the argument vector and executes them. If none are found, calls
 * Execute() */
int Run(std::vector<char*> argv)
{
	if (argv.size() < 1)
	{
		cout << "exit\n";
		exit(0);
	}
	if( !strcmp(argv[0], "exit") )
	{
		exitstatus = true;
		return 1;
	}
	else if ( !strcmp(argv[0], "cd") )
	{
		if (argv.size() < 2)
		{
			// fprintf(stderr, "Turtle: expected argument to \"cd\"\n");
			if (chdir(homedir) != 0) 
			{
				perror("Turtle: failed to change directory");
				return 1;
			}
			return 0;
		} 
		else 
		{
			if (chdir(argv[1]) != 0) 
			{
				perror("Turtle: failed to change directory");
				return 1;
			}
			return 0;
		}
	}
	else if ( !strcmp(argv[0], "history") )
	{
		int status;
		string histfile = HISTORY_FILE;
		histfile = homedir + histfile;
		char* s = new char[histfile.length()+1];
		strcpy(s, histfile.c_str());
		char cat[] = "cat";
		char flag[] = "-n";
		std::vector<char*> histcmd;
		histcmd.push_back(cat);
		histcmd.push_back(flag);
		histcmd.push_back(s);
		status = Execute(histcmd);
		delete[] s;
		return status;
	}
	else if ( !strcmp(argv[0], "math") )
	{
		if(argv.size() < 2)
		{
			fprintf(stderr, "Turtle: expected argument to \"math\"\n");
			return 1;
		}
		else
		{
			expressionToParse = argv[1];
			cout << expression() << endl;
			return 0;
		}
	}
	else if ( !strcmp(argv[0], "setmemlimit") )
	{
		if(argv.size() < 2)
		{
			fprintf(stderr, "Turtle: expected argument to \"setmemlimit\"\n");
			return 1;
		}
		else
		{
			expressionToParse = argv[1];
			MAXMEMLIMIT = expression();
			return 0;
		}
	}
	else if ( !strcmp(argv[0], "showmemlimit") )
	{
		cout << MAXMEMLIMIT << endl;
		return 0;
	}
	else if ( !strcmp(argv[0], "stopwatch") )
	{
		if(argv.size() < 2)
		{
			fprintf(stderr, "Turtle: expected argument to \"stopwatch\"\n");
			return 1;
		}
		else if ( !strcmp(argv[1], "start") )
		{
			globalwatch.start();
			return 0;
		}
		else if ( !strcmp(argv[1], "stop") )
		{
			cout << globalwatch.stop() << endl;
			return 0;
		}

	}
	else
	{
		return Execute(argv);
	}
	return 0;
}

/* MATH FUNCTIONS --- All math implementations for the math mode. */

float factorial(float x)
{
	float p = 1;
	float i;
	for(i = 1; i <= x; i+=1)
		p *= i;
	return p;
}

char peek()
{
	return *expressionToParse;
}

char get()
{
	return *expressionToParse++;
}

float number()
{
	float result = (float)(get() - '0');
	float denfactor = 1.0;
	float numfactor = 1.0;
	while (peek() >= '0' && peek() <= '9' || peek() == '.')
	{
			if(peek() == '.')
			{
				numfactor = 10.0;
				denfactor = 10.0;
				get();
				continue;
			}
		result = 10*result/numfactor + (float)(get() - '0')/denfactor;
		denfactor *= numfactor;
	}
	return result;
}

float factor()
{
	float result = 0;
	if (peek() >= '0' && peek() <= '9')
		result = number();
	else if (peek() == 'p')
	{
		get();
		if(peek() == 'i')
		{
			get();
			result = M_PI;
		}
	}
	else if (peek() == 'e')
	{
		get();
		result = M_E;
	}
	else if (peek() == '(')
	{
		get(); // '('
		result = expression();
		get(); // ')'
	}
	else if (peek() == '-')
	{
		get();
		result = -factor();
	}
	else if (peek() == '!')
	{
		get();
		result = factorial(factor());
	}
	 else if(peek() == 's')
	 {
		get();
		result = sin(factor());
	}
	 else if(peek() == 'c')
	 {	
		get();
		result = cos(factor());
	 }
	 else if(peek() == 't')
	 {	
		get();
		result = tan(factor());
	 }
	 else if(peek() == 'l')
	 {	
		get();
		result = log(factor());
	 }
	 else
	 {
	 	get();
	 }
	return result; 
}
float texp()
{
	float result = factor();
	while(peek() == '^')
	{
		get();
		result = pow(result, factor());
	}
	return result;
}

float term()
{
	float result = texp();
	while (peek() == '*' || peek() == '/')
		if (peek() == '*')
		{	
			get();
			result *= texp();
		}
		else if(peek() == '/')
		{	
			get();
			if(peek() == '/')
			{
				while(peek() == '/')
					get();
				result = (int)( result / texp());
			}
			else
				result /= texp();
		}
	return result;
}

float expression()
{
	float result = term();
	while (peek() == '+' || peek() == '-' )
		if (peek() == '+')
		{
			get();
			result += term();
		}
		else if(peek() == '-')
		{
			get();
			result -= term();
		}
		
	return result;
}
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <vector>
#include <fstream>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <math.h>

using namespace std;

#define HISTORY_FILE "/.turtlesh_history"

const char *homedir;

bool exitstatus = false;

char* expressionToParse;

enum ConcatType {NONE, PIPE, AREDIR, TREDIR, DOUBLEAMP, SEMICOLON}; 


std::vector<char*> ReadLine();
int WriteToHistory(std::vector<char*>);
int Execute(std::vector<char*>);
int PipedExecute(std::vector<char*>, std::vector<char*>);
void Print(std::vector<char*>);
void FreeArgs(std::vector<char*>);
int ParsedExecute(std::vector<char*>);
int RedirExecute(std::vector<char*>, std::vector<char*>, ConcatType);
int Run(std::vector<char*>);

float number();
char get();
char peek();
float factor();
float factorial();
float term();
float texp();
float expression();

int main()
{
	if ((homedir = getenv("HOME")) == NULL)
	{
	    homedir = getpwuid(getuid())->pw_dir;
	}
	int status = 0;
	std::vector<char*> argv;
	char prompt[1024];
	while(exitstatus == false)
	{
		cout << getcwd(prompt, 1024) << "$ ";
		argv = ReadLine();
		status = ParsedExecute(argv);
		// Print(argv);
		WriteToHistory(argv);
		FreeArgs(argv);
		argv.clear();
	}
	return 0;
}


vector<char*> ReadLine()
{
	string word;
	vector<char*> argv;
	char* s;
	while(cin >> word)
	{
		s = new char[word.length() + 1];
		strcpy(s, word.c_str());
		argv.push_back(s);
		if (cin.get() == '\n')
			break;
	}
	return argv;
}

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
		// cout << "Entering arg2\n";
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

void Print(std::vector<char*> argv)
{
	cout << "Executing: ";
	for (std::vector<char*>::iterator it = argv.begin(); it != argv.end(); ++it)
	{
		cout << *it << ' ';
	}
	cout << endl;
}

void FreeArgs(std::vector<char*> argv)
{
	for (std::vector<char*>::iterator i = argv.begin(); i != argv.end(); ++i)
	{
		delete[] *i;
	}
}

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
			fprintf(stderr, "Turtle: expected argument to \"cd\"\n");
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
	else
	{
		return Execute(argv);
	}
	return 0;
}


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
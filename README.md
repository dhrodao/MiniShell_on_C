# MiniShell

This program has been the final project for my Operating System course at university. 
It consists of programming a command interpreter (shell) for Linux. The shell should read lines from the standard input and execute commands. For reading from the standard input I've used **stdio** (fgets(3)).

* It's input format for a command line will be:

  >**command arg1 arg2 ...**

* In this program has also been implemented pipes, so you will be able to execute a pipeline:

  >**cmd1 arg11... | cmd2 arg21 arg22... | cmd3 arg31... | ...**

* A command also could have an input/output redirection of both:
  
  >**cmd1 args... < fin** / **cmd1 args... > fout** / **cmd1 args... < fin > fout**

* Implemented background:
  
  >**cmd1 args | cmd2 args... | ... &**
  
* Implemented environment variables:
  
  > cmd=ls<br>
  > arg=/tmp        **is equivalent to**    ls /tmp<br>
  > $cmd $arg<br>
  
* Additional functions:
  - For lines that aren't using input/output redirection or bg, if the line ends with "**HERE{**". In this case, the command will use as stdin the lines written by the user until the line is "**}**". Example: (**wc -l** will return **2**, as there is 2 lines as input for **cat** command)
    >cat | wc -l HERE{ <br>
    >  una y <br>
    >  otra lin. <br>
    >} <br>
    
  - Command ifok/ifnot, will execute the command written after it, if the previous command exit status is SUCCESS/FAILURE. Example: (if directory **/tmp** exists, **ls -l** will be executed)
    > test -e /tmp <br>
    > ifok ls -l /tmp
  
  - Globbing

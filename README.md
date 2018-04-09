# turtleshell
A simple shell made using C++
Recommended compile using g++-7 as some functions (std::quoted) requires C++14 standard

## Supports

Piping '|', Redirection ('>', '>>'), Conditional chaining '&&', Uncondition chaining ';'

Grouping of words with double quotes

## Extra shell builtins

stopwatch start|stop

history

math expression

Eg. math l(e^7)*s(pi/2)+cpi+!5-9//2+4.75/3

Translates to ln(e^7)*sin(pi/2) + cos(pi) + factorial(5) + floor(9/2) + 4.75/3


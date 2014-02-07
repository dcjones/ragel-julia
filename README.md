
This is a fork of [ragel](http://www.complang.org/ragel/) that adds a
[Julia](http://julialang.org/) backend to the yet unreleased ragel 7.0.

To build, you will need version 0.12.0 of [colm](http://www.complang.org/colm/).

The resulting Julia code relies on goto expressions, which are not yet in
Julia, but I have a pull request that adds them: https://github.com/JuliaLang/julia/pull/5699.

It also relies on the (Switch)[https://github.com/dcjones/Switch.jl] package
that adds C-style switch statements to Julia. That also uses gotos so its not yet
available from the package manager.

Here's a quick example:

```julia
using Switch

%%{
    machine Test1;

    action dgt      { @printf("DGT: %c\n", fc); }
    action dec      { @printf("DEC: .\n"); }
    action exp      { @printf("EXP: %c\n", fc); }
    action exp_sign { @printf("SGN: %c\n", fc); }
    action number   { println("NUMBER") }

    number = (
        [0-9]+ $dgt ( '.' @dec [0-9]+ $dgt )?
        ( [eE] ( [+\-] $exp_sign )? [0-9]+ $exp )?
    ) %number;

    main := ( number '\n' )*;
}%%


function parse(data::String)
    p = 0
    pe = length(data)
    %% write data;
    %% write init;
    %% write exec;
end

parse("1234.56")
```

Saving the above as `number.rl` and running  `ragel -U number.rl" will generate
a file `number.jl`, which is very efficient, pure Julia number parser.


                     Ragel State Machine Compiler -- README
                     ======================================

1. Build Requirements
---------------------

 * Make
 * g++

If you would like to modify Ragel and need to build Ragel's scanners and
parsers from the specifications then set "build_parsers=yes" the DIST file and
reconfigure. This variable is normally set to "no" in the distribution tarballs
and "yes" in version control. You will need the following programs:

 * ragel (the most recent version)
 * kelbt (the most recent version)

To build the user guide set "build_manual=yes" in the DIST file and
reconfigure. You will need the following extra programs:

 * fig2dev
 * pdflatex

2. Compilation and Installation
-------------------------------

Ragel uses autoconf and automake. 

$ ./configure --prefix=PREFIX
$ make
$ make install

CXX = g++
CXXFLAGS = -g -Wall -std=c++17 -I.

GENERATED_SRCS = parse.tab.cpp lex.yy.cpp grammar_symbols.cpp \
	ast.cpp ast_visitor.cpp
GENERATED_HDRS = parse.tab.h lex.yy.h grammar_symbols.h ast_visitor.h
SRCS = node.cpp node_base.cpp location.cpp treeprint.cpp \
	main.cpp context.cpp type.cpp symtab.cpp semantic_analysis.cpp \
	literal_value.cpp \
	yyerror.cpp exceptions.cpp cpputil.cpp \
	$(GENERATED_SRCS)
OBJS = $(SRCS:%.cpp=%.o)

PARSER_SRC = parse_buildast.y

EXE = nearly_cc

%.o : %.cpp
	$(CXX) $(CXXFLAGS) -c $*.cpp -o $*.o

all : $(EXE)

$(EXE) : $(GENERATED_SRCS) $(GENERATED_HDRS) $(OBJS)
	$(CXX) -o $@ $(OBJS)

parse.tab.h parse.tab.cpp : $(PARSER_SRC)
	bison -v --output-file=parse.tab.cpp --defines=parse.tab.h $(PARSER_SRC)

lex.yy.cpp lex.yy.h : lex.l
	flex --outfile=lex.yy.cpp --header-file=lex.yy.h lex.l

grammar_symbols.h grammar_symbols.cpp : $(PARSER_SRC) scan_grammar_symbols.rb
	./scan_grammar_symbols.rb < $(PARSER_SRC)

ast.cpp ast_visitor.h ast_visitor.cpp : ast.h gen_ast_code.rb
	./gen_ast_code.rb < ast.h

depend : $(GENERATED_SRCS)
	$(CXX) $(CXXFLAGS) -M $(SRCS) > depend.mak

depend.mak :
	touch $@

clean :
	rm -f *.o depend.mak $(GENERATED_SRCS) $(GENERATED_HDRS) \
		-f parse.output $(EXE)

include depend.mak

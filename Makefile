all: yank pop

yank: yank.cc common.cc
	g++ -o yank yank.cc common.cc

pop: pop.cc common.cc
	g++ -o pop pop.cc common.cc

nfqnl_test: nfqnl_test.cpp
	g++ -o nfqnl_test nfqnl_test.cpp -Wall -lnetfilter_queue -std=c++11

clean:
	rm nfqnl_test
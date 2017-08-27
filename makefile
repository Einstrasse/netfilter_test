nfqnl_test: nfqnl_test.c
	g++ -o nfqnl_test nfqnl_test.c -Wall -lnetfilter_queue

clean:
	rm nfqnl_test
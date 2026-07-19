.PHONY: all clean

all:
	$(CC) -c third_party/cJSON/cJSON.c -o cJSON.o -Ithird_party/cJSON
	$(CC) agent_lite.c cJSON.o -static -O3 -Wall -o agent_lite -Ithird_party/cJSON
	$(CC) simple_llm_server.c cJSON.o -static -O3 -Wall -o simple_llm_server -Ithird_party/cJSON -lm

clean:
	rm -f agent_lite simple_llm_server cJSON.o

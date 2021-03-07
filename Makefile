ALL:
	mkdir -p ./bin
	$(CXX) src/main.cpp src/Thrasher.cpp src/util.cpp -o bin/thrasher \
			-Ofast -std=c++11 -lpthread
	$(CXX) src/stream.cpp -o bin/stream -Ofast -std=c++11 -lpthread

clean:
	rm -rf bin

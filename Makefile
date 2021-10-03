ALL:
	mkdir -p ./bin
	$(CXX) src/main.cpp src/Thrasher.cpp src/util.cpp -o bin/thrasher \
			-O3 -std=c++11 -lpthread
	$(CXX) src/stream.cpp -o bin/stream -O3 -std=c++11 -lpthread
	$(CXX) src/rgen.cpp src/util.cpp -o bin/rgen -O3 -std=c++11

clean:
	rm -rf bin

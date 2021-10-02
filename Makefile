ALL:
	mkdir -p ./bin
	$(CXX) src/main.cpp src/Thrasher.cpp src/util.cpp -o bin/thrasher \
			-Ofast -std=c++11 -lpthread
	$(CXX) src/stream.cpp -o bin/stream -Ofast -std=c++11 -lpthread
	$(CXX) src/rgen.cpp src/util.cpp -o bin/rgen -Ofast -std=c++11

clean:
	rm -rf bin

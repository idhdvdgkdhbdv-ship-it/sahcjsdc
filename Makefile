LIBOTE_DIR = $(HOME)/libOTe
CXXFLAGS = -O3 -march=native --std=c++17 -pthread -DENABLE_SIMPLESTOT
INCLUDES = -Ioprf/PSI/include -I$(LIBOTE_DIR) -I$(LIBOTE_DIR)/cryptoTools -I$(LIBOTE_DIR)/cryptoTools/thirdparty/linux/miracl
LIBDIRS = -L$(LIBOTE_DIR)/lib -L/usr/local/lib
LIBS = -lntl -lgmp -llibOTe -lcryptoTools -lSimplestOT -lmiracl -pthread

all:
	g++ -c oprf/oprf.cpp oprf/PSI/src/PsiReceiver.cpp oprf/PSI/src/PsiSender.cpp oprf/PSI/src/utils.cpp $(INCLUDES) $(CXXFLAGS)
	g++ generate_random_sets.cpp -lntl -lgmp -pthread -O3 -march=native --std=c++17 -o generate_random_sets
	g++ P1_private_set_intersection.cpp oprf.o PsiSender.o PsiReceiver.o utils.o $(INCLUDES) $(LIBDIRS) $(LIBS) $(CXXFLAGS) -o P1_private_set_intersection
	g++ P2_private_set_intersection.cpp oprf.o PsiSender.o PsiReceiver.o utils.o $(INCLUDES) $(LIBDIRS) $(LIBS) $(CXXFLAGS) -o P2_private_set_intersection
	g++ Q_private_set_intersection.cpp -lntl -lgmp -pthread -O3 -march=native --std=c++17 -o Q_private_set_intersection

clean:
	rm -f *.o generate_random_sets P1_private_set_intersection P2_private_set_intersection Q_private_set_intersection

.PHONY: all clean

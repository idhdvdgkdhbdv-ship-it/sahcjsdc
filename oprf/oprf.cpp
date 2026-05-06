#include "oprf.hpp"

using namespace std;
using namespace PSI;

u64 getWidth(u64 size) {
	u64 width;

	if (size > (1 << 24)) {
		std::cerr << "Set size is too large. Unsupported!" << std::endl;
	}
	if (size <= (1 << 24)) {
		width = 633;
	}
	if (size <= (1 << 22)) {
		width = 627;
	}
	if (size <= (1 << 20)) {
		width = 621;
	}
	if (size <= (1 << 18)) {
		width = 615;
	}
	if (size <= (1 << 16)) {
		width = 609;
	}

	return width;
}

u64 getLogHeight(u64 size) {
	u64 logHeight;

	for (int i = 63; i >= 1; i--) {
		if (size <= (1 << i)) {
			logHeight = i;
		}
	}

	return logHeight;
}

void runSender(std::string ip, std::string name, u64 senderSize, u64 receiverSize, vector<block> senderSet, u64 hashLengthInBytes, u8* result, u64* totalData, int seed1, int seed2) {
	u64 maxsize = std::max(senderSize, receiverSize);
	u64 width = getWidth(maxsize);
	u64 logHeight = getLogHeight(maxsize);
	u64 height = 1 << logHeight;
	u64 bucket1 = 1 << 8;
	u64 bucket2 = 1 << 8;

	IOService ios;
	Endpoint ep(ios, ip, EpMode::Server, name);
	Channel ch = ep.addChannel();

	PRNG prng(oc::toBlock(seed1));
	block commonSeed = oc::toBlock(seed2);

	PsiSender psiSender;
	psiSender.run(prng, ch, commonSeed, senderSize, receiverSize, height, logHeight, width, senderSet, hashLengthInBytes, 32, bucket1, bucket2, result, totalData);

	ch.close();
	ep.stop();
	ios.stop();
}

void runReceiver(std::string ip, std::string name, u64 senderSize, u64 receiverSize, vector<block> receiverSet, u64 hashLengthInBytes, u8* result, u64* totalData, int seed1, int seed2) {
	u64 maxsize = std::max(senderSize, receiverSize);
	u64 width = getWidth(maxsize);
	u64 logHeight = getLogHeight(maxsize);
	u64 height = 1 << logHeight;
	u64 bucket1 = 1 << 8;
	u64 bucket2 = 1 << 8;

	IOService ios;
	Endpoint ep(ios, ip, EpMode::Client, name);
	Channel ch = ep.addChannel();

	PRNG prng(oc::toBlock(seed1));
	block commonSeed = oc::toBlock(seed2);
	
	PsiReceiver psiReceiver;
	psiReceiver.run(prng, ch, commonSeed, senderSize, receiverSize, height, logHeight, width, receiverSet, hashLengthInBytes, 32, bucket1, bucket2, result, totalData);
	
	ch.close();
	ep.stop();
	ios.stop();
}

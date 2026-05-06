#ifndef OPRF_WRAPPER_H
#define OPRF_WRAPPER_H

#include "PSI/include/Defines.h"
#include "PSI/include/utils.h"
#include "PSI/include/PsiSender.h"
#include "PSI/include/PsiReceiver.h"

#include <cryptoTools/Network/IOService.h>
#include <cryptoTools/Network/Endpoint.h>
#include <cryptoTools/Network/Channel.h>
#include <cryptoTools/Common/CLP.h>
#include <cryptoTools/Common/Log.h>

#include <vector>
#include <string>

using namespace std;
using namespace PSI;

void runSender(std::string ip, std::string name, u64 senderSize, u64 receiverSize, vector<block> senderSet, u64 hashLengthInBytes, u8* result, u64* totalData, int seed1, int seed2);

void runReceiver(std::string ip, std::string name, u64 senderSize, u64 receiverSize, vector<block> receiverSet, u64 hashLengthInBytes, u8* result, u64* totalData, int seed1, int seed2);

#endif

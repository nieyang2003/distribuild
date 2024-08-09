#pragma once
#include <Poco/Util/ServerApplication.h>

namespace distribuild {

class TerminationWaiter : public Poco::Util::ServerApplication {
 public:
  int main(const std::vector<std::string>& args) override {
    waitForTerminationRequest();
	return 0;
  }
};

}
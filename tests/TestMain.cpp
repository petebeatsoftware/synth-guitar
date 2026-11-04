#include "TestHelpers.h"

#include <iostream>

namespace
{
class Runner final : public juce::UnitTestRunner
{
	void logMessage(const juce::String& message) override
	{
		std::cout << message << '\n';
	}
};
}

int main(int argc, char* argv[])
{
	juce::ScopedJuceInitialiser_GUI initialiseJuce;
	Runner runner;
	runner.setAssertOnFailure(false);
	juce::Array<juce::UnitTest*> selected;
	for (auto* suite : juce::UnitTest::getAllTests())
		if (argc == 1 || suite->getCategory() == juce::String(argv[1]))
			selected.add(suite);

	if (argc > 2 || selected.isEmpty())
	{
		std::cerr << "Expected an optional valid test category\n";
		return 1;
	}

	runner.runTests(selected, 1234);
	int failures = 0;
	int passes = 0;
	for (int i = 0; i < runner.getNumResults(); ++i)
	{
		failures += runner.getResult(i)->failures;
		passes += runner.getResult(i)->passes;
	}
	std::cout << passes << " assertions passed; " << failures << " failed\n";
	return failures == 0 && runner.getNumResults() > 0 ? 0 : 1;
}

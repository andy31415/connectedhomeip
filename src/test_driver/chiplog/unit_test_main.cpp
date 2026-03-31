/*
 *    Copyright (c) 2026 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include <string_view>

#include <lib/support/CHIPArgParser.hpp>
#include <lib/support/CHIPMem.h>
#include <lib/support/CHIPPlatformMemory.h>
#include <lib/support/logging/CHIPLogging.h>

#include "pw_unit_test/event_handler.h"
#include "pw_unit_test/framework.h"

using namespace chip;
using namespace pw::unit_test;

namespace {

bool gQuiet = false;

#define kOptQuiet 'q'

ArgParser::OptionDef gProgramCustomOptionDefs[] = {
    { "quiet", ArgParser::kNoArgument, kOptQuiet },
    {},
};

const char gProgramCustomOptionHelp[] = "  --quiet\n"
                                        "       Oputput only failures/assert errors \n"
                                        "\n";

bool HandleCustomOption(const char * aProgram, ArgParser::OptionSet * aOptions, int aIdentifier, const char * aName,
                        const char * aValue)
{
    switch (aIdentifier)
    {
    case kOptQuiet:
        gQuiet = true;
        break;
    default:
        ArgParser::PrintArgError("%s: INTERNAL ERROR: Unhandled option: %s\n", aProgram, aName);
        return false;
    }

    return true;
}

ArgParser::OptionSet gProgramCustomOptions = { HandleCustomOption, gProgramCustomOptionDefs, "GENERAL OPTIONS",
                                               gProgramCustomOptionHelp };
ArgParser::OptionSet * gAllOptions[]       = { &gProgramCustomOptions, nullptr };

class ChipLogHandler : public pw::unit_test::EventHandler
{
public:
    ChipLogHandler(bool quiet) : mQuiet(quiet) {}

    void TestProgramStart(const ProgramSummary & program_summary) override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[==========] Running %d tests from %d test suite%s.", program_summary.tests_to_run,
                        program_summary.test_suites, program_summary.test_suites > 1 ? "s" : "");
    }

    void EnvironmentsSetUpEnd() override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[----------] Global test environments setup.");
    }

    void TestSuiteStart(const TestSuite & test_suite) override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[----------] %d tests from %s.", test_suite.test_to_run_count, test_suite.name);
    }

    void TestSuiteEnd(const TestSuite & test_suite) override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[----------] %d tests from %s.", test_suite.test_to_run_count, test_suite.name);
    }

    /// Called after environment teardown for each iteration of tests ends.
    void EnvironmentsTearDownEnd() override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[----------] Global test environments tear-down.");
    }

    /// Called after all test activities have ended.
    void TestProgramEnd(const ProgramSummary & program_summary) override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[==========] %d / %d tests from %d test suite%s ran.",
                        program_summary.tests_to_run - program_summary.tests_summary.skipped_tests -
                            program_summary.tests_summary.disabled_tests,
                        program_summary.tests_to_run, program_summary.test_suites, program_summary.test_suites > 1 ? "s" : "");
        ChipLogProgress(Test, "[  PASSED  ] %d test(s).", program_summary.tests_summary.passed_tests);

        if (program_summary.tests_summary.skipped_tests || program_summary.tests_summary.disabled_tests)
        {
            ChipLogProgress(Test, "[ DISABLED ] %d test(s).",
                            program_summary.tests_summary.skipped_tests + program_summary.tests_summary.disabled_tests);
        }

        if (program_summary.tests_summary.failed_tests)
        {
            ChipLogError(Test, "[  FAILED  ] %d test(s).", program_summary.tests_summary.failed_tests);
        }
    }

    /// Called before all tests are run.
    void RunAllTestsStart() override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[==========] Running all tests.");
    }

    /// Called after all tests are run.
    void RunAllTestsEnd(const RunTestsSummary & run_tests_summary) override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[==========] Done running all tests.");
        ChipLogProgress(Test, "[  PASSED  ] %d test(s).", run_tests_summary.passed_tests);

        if (run_tests_summary.skipped_tests)
        {
            ChipLogProgress(Test, "[ DISABLED ] %d test(s).", run_tests_summary.skipped_tests);
        }
        if (run_tests_summary.failed_tests)
        {
            ChipLogError(Test, "[  FAILED  ] %d test(s).", run_tests_summary.failed_tests);
        }
    }

    void TestCaseStart(const TestCase & test_case) override
    {
        VerifyOrReturn(!mQuiet);
        ChipLogProgress(Test, "[ RUN      ] %s.%s", test_case.suite_name, test_case.test_name);
    }

    void TestCaseEnd(const TestCase & test_case, TestResult result) override
    {
        switch (result)
        {
        case TestResult::kSuccess:
            if (!mQuiet)
            {
                ChipLogProgress(Test, "[       OK ] %s.%s", test_case.suite_name, test_case.test_name);
            }
            break;
        case TestResult::kFailure:
            ChipLogError(Test, "[  FAILED  ] %s.%s", test_case.suite_name, test_case.test_name);
            break;
        case TestResult::kSkipped:
            if (!mQuiet)
            {
                ChipLogProgress(Test, "[ DISABLED ] %s.%s", test_case.suite_name, test_case.test_name);
            }
            break;
        }
    }

    /// Called when a disabled test case is encountered.
    void TestCaseDisabled(const TestCase & test) override
    {
        ChipLogProgress(Test, "Skipping disabled test %s.%s", test.suite_name, test.test_name);
    }

    /// Called after each expect or assert statement within a test case with the
    /// result.
    void TestCaseExpect(const TestCase & test_case, const TestExpectation & expectation) override
    {
        VerifyOrReturn(!expectation.success);

        ChipLogError(Test, "%s:%d: Failure", expectation.file_name, expectation.line_number);
        ChipLogError(Test, "      Expected: %s", expectation.expression);
        ChipLogError(Test, "      Actual:   %s", expectation.evaluated_expression);
    }

private:
    bool mQuiet;
};

} // namespace

int main(int argc, char ** argv)
{
    SuccessOrDie(Platform::MemoryInit());
    if (!ArgParser::ParseArgs(argv[0], argc, argv, gAllOptions))
    {
        return 1;
    }

    // Make the binary compatible with pw_unit_test:googletest. Has no effect
    // when using pw_unit_test:light.
    testing::InitGoogleTest(&argc, argv);
    ChipLogHandler handler(gQuiet);

    pw::unit_test::RegisterEventHandler(&handler);
    return RUN_ALL_TESTS();
}

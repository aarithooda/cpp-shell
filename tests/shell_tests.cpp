#include "parser.h"
#include "process.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace fs = std::filesystem;

void testQuotesAndOperators() {
    int status = 0;
    auto tokens = tokenize("echo 'one two' \"three;four\"; pwd && echo done", status);
    assert(status == 0);
    assert(tokens.size() == 8);
    assert(tokens[0].text == "echo");
    assert(tokens[1].text == "one two");
    assert(tokens[2].text == "three;four");
    assert(tokens[3].kind == TokenKind::command_separator);
    assert(tokens[5].kind == TokenKind::conditional_and);
}

void testRedirectionAndArguments() {
    int status = 0;
    auto tokens = tokenize("tool first > out.txt second < in.txt", status);
    Command command = parse(tokens, status);
    assert(status == 0);
    assert(command.program == "tool");
    assert(command.arguments.size() == 2);
    assert(command.arguments[1] == "second");
    assert(command.output_file == "out.txt");
    assert(command.input_file == "in.txt");
}

void testMalformedRedirection() {
    int status = 0;
    auto tokens = tokenize("tool >", status);
    Command command = parse(tokens, status);
    assert(status != 0);
    assert(command.program.empty());
}

void testEscapingAndVariableExpansion() {
    int status = 0;
    auto tokens = tokenize("echo hello\\ world '$CPP_SHELL_TEST_VAR' $CPP_SHELL_TEST_VAR", status);
    assert(status == 0);
    assert(tokens.size() == 4);
    assert(tokens[1].text == "hello world");
    assert(tokens[2].text == "$CPP_SHELL_TEST_VAR");
    _putenv_s("CPP_SHELL_TEST_VAR", "after-tokenize");
    expand_variables(tokens);
    assert(tokens[3].text == "after-tokenize");
}

void testProcessRedirection() {
    fs::path input_path = fs::temp_directory_path() / fs::u8path(u8"cpp-shell-ü-input.txt");
    fs::path output_path = fs::temp_directory_path() / fs::u8path(u8"cpp-shell-ü-output.txt");
    {
        std::ofstream input(input_path, std::ios::binary);
        input << "needle\r\nother\r\n";
    }
    {
        std::ofstream output(output_path, std::ios::binary);
        output << "first\r\n";
    }

    ProcessResult result = create_process("findstr.exe", { "/c:needle" }, input_path.u8string(), output_path.u8string(), true);
    assert(result.success);
    assert(result.exitcode == 0);
    std::ifstream output(output_path, std::ios::binary);
    std::string line;
    std::getline(output, line);
    assert(line == "first\r");
    std::getline(output, line);
    assert(line == "needle\r");
    output.close();

    fs::remove(input_path);
    fs::remove(output_path);
}

void testProcessExitCode() {
    ProcessResult result = create_process("cmd.exe", { "/d", "/c", "exit 7" });
    assert(result.success);
    assert(result.exitcode == 7);
}

void testProcessPipeline() {
    SECURITY_ATTRIBUTES attributes{ sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE pipe_input = NULL;
    HANDLE pipe_output = NULL;
    assert(CreatePipe(&pipe_input, &pipe_output, &attributes, 0));

    fs::path output_path = fs::temp_directory_path() / fs::u8path(u8"cpp-shell-pipeline-ü.txt");
    RunningProcess writer = launch_process("cmd.exe", { "/d", "/c", "for /L %i in (1,1,5000) do @echo needle" }, std::nullopt, std::nullopt, false, NULL, pipe_output);
    RunningProcess reader = launch_process("findstr.exe", { "needle" }, std::nullopt, output_path.u8string(), false, pipe_input, NULL);
    CloseHandle(pipe_input);
    CloseHandle(pipe_output);

    ProcessResult writer_result = wait_process(writer);
    ProcessResult reader_result = wait_process(reader);
    assert(writer_result.success && writer_result.exitcode == 0);
    assert(reader_result.success && reader_result.exitcode == 0);

    std::ifstream output(output_path, std::ios::binary);
    std::string line;
    size_t count = 0;
    while (std::getline(output, line)) {
        assert(line == "needle\r");
        count++;
    }
    assert(count == 5000);
    output.close();
    fs::remove(output_path);
}

int main() {
    testQuotesAndOperators();
    testRedirectionAndArguments();
    testMalformedRedirection();
    testEscapingAndVariableExpansion();
    testProcessRedirection();
    testProcessExitCode();
    testProcessPipeline();
}

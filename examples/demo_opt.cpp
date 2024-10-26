#include <iostream>
#include "cmdline.h"
#include "models/opt/modeling_opt.hpp"
#include "models/opt/tokenization_opt.hpp"
#include "processor/PostProcess.hpp"

using namespace mllm;

int main(int argc, char **argv) {
    cmdline::parser cmdParser;
    cmdParser.add<string>("vocab", 'v', "specify mllm tokenizer model path", false, "../vocab/opt_vocab.mllm");
    cmdParser.add<string>("merge", 'e', "specify mllm merge path", false, "../vocab/opt_merges.txt");
    cmdParser.add<string>("model", 'm', "specify mllm model path", false, "../models/opt-1.3b-fp32.mllm");
    cmdParser.add<int>("limits", 'l', "max KV cache size", false, 400);
    cmdParser.add<int>("thread", 't', "num of threads", false, 4);
    cmdParser.parse_check(argc, argv);

    string vocab_path = cmdParser.get<string>("vocab");
    string merge_path = cmdParser.get<string>("merge");
    string model_path = cmdParser.get<string>("model");
    int tokens_limit = cmdParser.get<int>("limits");
    CPUBackend::cpu_threads = cmdParser.get<int>("thread");

    auto tokenizer = OPTTokenizer(vocab_path, merge_path);

    OPTConfig config(tokens_limit, "1.3B");

    auto model = OPTModel(config);

    model.load(model_path);

    vector<string> in_strs = {
        " Hello, who are you?",
        " What can you do?",
        "Please introduce Beijing University of Posts and Telecommunications."};

    for (int i = 0; i < in_strs.size(); ++i) {
        auto in_str = in_strs[i];
        std::cout << "[Q] " << in_str << std::endl;
        auto input_tensor = tokenizer.tokenize(in_str);
        std::cout << "[A] " << std::flush;

        for (int step = 0; step < 50; step++) {
            auto result = model({input_tensor});
            auto [out_string, out_token] = tokenizer.detokenize(result[0]);
            auto [not_end, output_string] = tokenizer.postprocess(out_string);
            if (!not_end) { break; }
            std::cout << output_string << std::flush;
            chatPostProcessing(out_token, input_tensor, {});
        }
        printf("\n");
    }

    return 0;
}

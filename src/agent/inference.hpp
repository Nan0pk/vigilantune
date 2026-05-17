#pragma once
#include <vector>
#include <string>
#include <memory>
#include <map>

// Note: Requires ONNX Runtime headers in include path
#include <onnxruntime_cxx_api.h>

namespace wspa {
    class InferenceManager {
    public:
        InferenceManager(const std::wstring& model_path);
        ~InferenceManager();

        // Runs inference on a vector of system metrics
        // Inputs: [CPU_Util, Queue_Len, Foreground_Hash, etc.]
        std::vector<float> run_inference(const std::vector<float>& input_tensor_values);

    private:
        Ort::Env m_env;
        std::unique_ptr<Ort::Session> m_session;
        Ort::MemoryInfo m_memory_info;

        std::vector<int64_t> m_input_node_dims;
        std::vector<const char*> m_input_node_names;
        std::vector<const char*> m_output_node_names;
    };
}

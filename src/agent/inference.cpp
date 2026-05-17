#include "inference.hpp"
#include <iostream>
#include <numeric>

namespace wspa {
    InferenceManager::InferenceManager(const std::wstring& model_path) 
        : m_env(ORT_LOGGING_LEVEL_WARNING, "WinSCADA"),
          m_memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
        
        try {
            // Check if file exists
            DWORD dwAttrib = GetFileAttributesW(model_path.c_str());
            if (dwAttrib == INVALID_FILE_ATTRIBUTES || (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
                std::cerr << "[Inference] Model file not found: " << std::string(model_path.begin(), model_path.end()) << ". Falling back to deterministic logic." << std::endl;
                return;
            }

            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            m_session = std::make_unique<Ort::Session>(m_env, model_path.c_str(), session_options);

            // Hardcoded for the prototype model: [1, 5] tensor input
            m_input_node_dims = {1, 5}; 
            m_input_node_names = {"input"};
            m_output_node_names = {"output"};

            std::cout << "[Inference] ONNX Session initialized from: " << std::string(model_path.begin(), model_path.end()) << std::endl;
        } catch (const Ort::Exception& e) {
            std::cerr << "[Inference] Error: " << e.what() << std::endl;
        }
    }

    InferenceManager::~InferenceManager() {}

    std::vector<float> InferenceManager::run_inference(std::vector<float> input_tensor_values) {
        if (!m_session) return {};

        try {
            size_t input_tensor_size = input_tensor_values.size();
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                m_memory_info, input_tensor_values.data(), input_tensor_size, 
                m_input_node_dims.data(), m_input_node_dims.size());

            auto output_tensors = m_session->Run(
                Ort::RunOptions{nullptr}, 
                m_input_node_names.data(), &input_tensor, 1, 
                m_output_node_names.data(), 1);

            float* floatarr = output_tensors.front().GetTensorMutableData<float>();
            size_t output_size = output_tensors.front().GetTensorTypeAndShapeInfo().GetElementCount();

            return std::vector<float>(floatarr, floatarr + output_size);
        } catch (const Ort::Exception& e) {
            std::cerr << "[Inference] Runtime Error: " << e.what() << std::endl;
            return {};
        }
    }
}

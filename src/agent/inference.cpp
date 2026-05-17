#include "inference.hpp"
#include <iostream>
#include <numeric>
#include <windows.h> // Significant #17: Explicit Win32 include

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

            // Fix for Significant #13: Query ONNX node names dynamically
            Ort::AllocatorWithDefaultOptions allocator;
            for (size_t i = 0; i < m_session->GetInputCount(); ++i) {
                auto name = m_session->GetInputNameAllocated(i, allocator);
                m_input_node_names_raw.push_back(name.get());
            }
            for (size_t i = 0; i < m_session->GetOutputCount(); ++i) {
                auto name = m_session->GetOutputNameAllocated(i, allocator);
                m_output_node_names_raw.push_back(name.get());
            }

            // Hardcoded for the prototype model: [1, 5] tensor input
            m_input_node_dims = {1, 5}; 

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

            // Convert raw strings to C-strings for API call
            std::vector<const char*> input_names_c;
            for (const auto& name : m_input_node_names_raw) input_names_c.push_back(name.c_str());
            std::vector<const char*> output_names_c;
            for (const auto& name : m_output_node_names_raw) output_names_c.push_back(name.c_str());

            auto output_tensors = m_session->Run(
                Ort::RunOptions{nullptr}, 
                input_names_c.data(), &input_tensor, 1, 
                output_names_c.data(), output_names_c.size());

            float* floatarr = output_tensors.front().GetTensorMutableData<float>();
            size_t output_size = output_tensors.front().GetTensorTypeAndShapeInfo().GetElementCount();

            return std::vector<float>(floatarr, floatarr + output_size);
        } catch (const Ort::Exception& e) {
            std::cerr << "[Inference] Runtime Error: " << e.what() << std::endl;
            return {};
        }
    }
}

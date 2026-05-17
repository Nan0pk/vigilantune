#include "inference.hpp"
#include <iostream>
#include <numeric>
#include <windows.h>
#include <bcrypt.h>
#include <iomanip>
#include <sstream>
#include "../shared/config.hpp"

#pragma comment(lib, "bcrypt.lib")

namespace wspa {
    InferenceManager::InferenceManager(const std::wstring& model_path) 
        : m_env(ORT_LOGGING_LEVEL_WARNING, "WinSCADA"),
          m_memory_info(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
        
        try {
            // Security #2: Model Integrity Verification (SHA-256)
            if (!verify_model_hash(model_path, config::EXPECTED_MODEL_HASH)) {
                std::cerr << "[Security] Model verification failed. Load aborted." << std::endl;
                return;
            }

            Ort::SessionOptions session_options;
            session_options.SetIntraOpNumThreads(1);
            session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            m_session = std::make_unique<Ort::Session>(m_env, model_path.c_str(), session_options);

            Ort::AllocatorWithDefaultOptions allocator;
            for (size_t i = 0; i < m_session->GetInputCount(); ++i) {
                auto name = m_session->GetInputNameAllocated(i, allocator);
                m_input_node_names_raw.push_back(name.get());
            }
            for (size_t i = 0; i < m_session->GetOutputCount(); ++i) {
                auto name = m_session->GetOutputNameAllocated(i, allocator);
                m_output_node_names_raw.push_back(name.get());
            }

            m_input_node_dims = {1, 5}; 

            std::cout << "[Inference] ONNX Session verified and initialized." << std::endl;
        } catch (const Ort::Exception& e) {
            std::cerr << "[Inference] ORT Exception: " << e.what() << std::endl;
        }
    }

    InferenceManager::~InferenceManager() {}

    bool InferenceManager::verify_model_hash(const std::wstring& path, const std::string& expected_hex_hash) {
        if (expected_hex_hash.empty()) {
            std::cout << "[Security] WARNING: No expected hash provided. Running in insecure mode." << std::endl;
            return true;
        }

        HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        BCRYPT_ALG_HANDLE hAlg = NULL;
        BCRYPT_HASH_HANDLE hHash = NULL;
        NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
        
        DWORD cbHashObject = 0, cbData = 0, cbHash = 0;
        status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0);
        status = BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0);

        std::vector<BYTE> hashObject(cbHashObject);
        std::vector<BYTE> hash(cbHash);
        status = BCryptCreateHash(hAlg, &hHash, hashObject.data(), cbHashObject, NULL, 0, 0);

        BYTE buffer[4096];
        DWORD bytesRead;
        while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
            BCryptHashData(hHash, buffer, bytesRead, 0);
        }

        BCryptFinishHash(hHash, hash.data(), cbHash, 0);

        std::stringstream ss;
        for (BYTE b : hash) ss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
        std::string actual_hash = ss.str();

        CloseHandle(hFile);
        BCryptDestroyHash(hHash);
        BCryptCloseAlgorithmProvider(hAlg, 0);

        if (actual_hash != expected_hex_hash) {
            std::cerr << "[Security] Hash mismatch!" << std::endl;
            std::cerr << "  Expected: " << expected_hex_hash << std::endl;
            std::cerr << "  Actual:   " << actual_hash << std::endl;
            return false;
        }

        std::cout << "[Security] SHA-256 Verified: " << actual_hash.substr(0, 8) << "..." << std::endl;
        return true;
    }

    std::vector<float> InferenceManager::run_inference(std::vector<float> input_tensor_values) {
        if (!m_session) return {};

        try {
            size_t input_tensor_size = input_tensor_values.size();
            Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
                m_memory_info, input_tensor_values.data(), input_tensor_size, 
                m_input_node_dims.data(), m_input_node_dims.size());

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

import torch
import torch.nn as nn
import torch.optim as optim
import pandas as pd
import numpy as np
import os
import argparse
import hashlib

# Define the PowerMLP Model
class PowerMLP(nn.Module):
    def __init__(self, input_size=5, hidden_sizes=[16, 8], output_size=1):
        super(PowerMLP, self).__init__()
        self.network = nn.Sequential(
            nn.Linear(input_size, hidden_sizes[0]),
            nn.ReLU(),
            nn.Linear(hidden_sizes[0], hidden_sizes[1]),
            nn.ReLU(),
            nn.Linear(hidden_sizes[1], output_size),
            nn.Sigmoid() # Scale output 0.0 - 1.0 (will multiply by 100 later)
        )

    def forward(self, x):
        return self.network(x) * 100.0

def generate_synthetic_data(num_samples=1000):
    """Generates a synthetic dataset for bootstrapping."""
    print(f"Generating {num_samples} synthetic samples...")
    data = []
    for _ in range(num_samples):
        cpu = np.random.uniform(0, 100)
        queue = np.random.randint(0, 50)
        # SSS calculation matches C++ logic roughly
        queue_norm = np.clip(np.log1p(queue) / np.log1p(50.0) * 100.0, 0, 100)
        sss = (cpu * 0.35) + (queue_norm * 0.50) + (np.random.uniform(0, 20) * 0.15)
        app_hash = np.random.uniform(0, 1.0)
        last_adj = np.random.uniform(0, 100)
        
        # Target logic: If SSS > 70, Boost = 100; If SSS < 30, Boost = 0; else 50
        if sss > 70:
            target = 100.0
        elif sss < 30:
            target = 0.0
        else:
            target = 50.0
        
        target += np.random.normal(0, 5) # Add noise
        target = np.clip(target, 0, 100)
        
        data.append([cpu, queue, sss, app_hash, last_adj, target])
    
    columns = ["CPU_Utilization", "Thread_Queue_Length", "Stress_Score", "App_Hash", "Last_Adjustment", "PerformanceBoost_Label"]
    return pd.DataFrame(data, columns=columns)

def train_model(data_path, epochs=100, batch_size=32):
    if os.path.exists(data_path) and os.path.getsize(data_path) > 100:
        print(f"Loading dataset from {data_path}...")
        df = pd.read_csv(data_path)
    else:
        df = generate_synthetic_data()

    X = df.iloc[:, :5].values.astype(np.float32)
    y = df.iloc[:, 5].values.astype(np.float32).reshape(-1, 1)

    dataset = torch.utils.data.TensorDataset(torch.from_numpy(X), torch.from_numpy(y))
    loader = torch.utils.data.DataLoader(dataset, batch_size=batch_size, shuffle=True)

    model = PowerMLP()
    optimizer = optim.Adam(model.parameters(), lr=0.001)
    criterion = nn.MSELoss()

    model.train()
    for epoch in range(epochs):
        total_loss = 0
        for batch_x, batch_y in loader:
            optimizer.zero_grad()
            outputs = model(batch_x)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()
            total_loss += loss.item()
        
        if (epoch + 1) % 10 == 0:
            print(f"Epoch [{epoch+1}/{epochs}], Loss: {total_loss/len(loader):.4f}")

    return model

def export_onnx(model, output_path):
    print(f"Exporting model to {output_path}...")
    model.eval()
    dummy_input = torch.randn(1, 5)
    
    torch.onnx.export(
        model,
        dummy_input,
        output_path,
        export_params=True,
        opset_version=11,
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={'input': {0: 'batch_size'}, 'output': {0: 'batch_size'}}
    )
    
    # Calculate SHA-256 for config.hpp
    sha256_hash = hashlib.sha256()
    with open(output_path, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    
    print("-" * 30)
    print("EXPORT COMPLETE")
    print(f"SHA-256: {sha256_hash.hexdigest()}")
    print("Copy the hash above into config::EXPECTED_MODEL_HASH in config.hpp")
    print("-" * 30)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="nanoloop Model Training Pipeline")
    parser.add_argument("--data", type=str, default="models/telemetry_log.csv", help="Path to telemetry log")
    parser.add_argument("--output", type=str, default="models/power_model.onnx", help="Output ONNX path")
    parser.add_argument("--epochs", type=int, default=100)
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.output), exist_ok=True)

    trained_model = train_model(args.data, epochs=args.epochs)
    export_onnx(trained_model, args.output)

import re
import matplotlib.pyplot as plt
import pandas as pd

# Function to parse the MPI benchmark results from a text file
def parse_mpi_results(filename):
    data = {
        "Buffer Size (bytes)": [],
        "Item Count": [],
        "Bcast Avg Time (μs)": [],
        "Bcast (Merkle) Avg Time (μs)": []
    }

    # Regular expressions to match the relevant lines
    buffer_and_item_pattern = re.compile(r"Bcast Buffer size: (\d+) bytes, item count: (\d+)")
    send_recv_pattern = re.compile(r"Bcast \s+Size:\s+\d+ bytes\s+Time:\s+min=\s*[\d.]+ μs avg=\s*([\d.]+) μs")
    send_recv_merkle_pattern = re.compile(r"Bcast \(Merkle\)\s+Size:\s+\d+ bytes\s+Time:\s+min=\s*[\d.]+ μs avg=\s*([\d.]+) μs")

    current_buffer_size = None
    current_item_count = None

    with open(filename, "r") as file:
        for line in file:
            # Match buffer size and item count
            buffer_and_item_match = buffer_and_item_pattern.search(line)
            if buffer_and_item_match:
                current_buffer_size = int(buffer_and_item_match.group(1))
                current_item_count = int(buffer_and_item_match.group(2))

            # Match Send-Recv avg time
            send_recv_match = send_recv_pattern.search(line)
            if send_recv_match and current_buffer_size and current_item_count:
                data["Buffer Size (bytes)"].append(current_buffer_size)
                data["Item Count"].append(current_item_count)
                data["Bcast Avg Time (μs)"].append(float(send_recv_match.group(1)))
                data["Bcast (Merkle) Avg Time (μs)"].append(None)  # Placeholder for Merkle timing

            # Match Send-Recv (Merkle) avg time
            send_recv_merkle_match = send_recv_merkle_pattern.search(line)
            if send_recv_merkle_match and current_buffer_size and current_item_count:
                # Find the last entry that matches the buffer size and item count, and update Merkle time
                for i in range(len(data["Buffer Size (bytes)"])-1, -1, -1):
                    if data["Buffer Size (bytes)"][i] == current_buffer_size and data["Item Count"][i] == current_item_count:
                        data["Bcast (Merkle) Avg Time (μs)"][i] = float(send_recv_merkle_match.group(1))
                        break

    # Create a DataFrame from the parsed data
    df = pd.DataFrame(data)
    return df

# Parse the MPI results from the text file
filename = "bcast_64.txt"  # Replace with your file name
df = parse_mpi_results(filename)

# Plotting
plt.figure(figsize=(7, 6))

# Unique item counts to plot separately
item_counts = df["Item Count"].unique()

count = 0

# Plot for Send-Recv and Send-Recv (Merkle) with each item count
for item_count in item_counts:
    df_item = df[df["Item Count"] == item_count]
    print(df_item)
    if count == 0:
      # Plot Send-Recv for each item count
      plt.plot(df_item["Buffer Size (bytes)"], df_item["Bcast Avg Time (μs)"], 'o-', label=f"Bcast Avg Time")
      count += 1
    
    # Plot Send-Recv (Merkle) for each item count
    plt.plot(df_item["Buffer Size (bytes)"], df_item["Bcast (Merkle) Avg Time (μs)"], 's-', label=f"Bcast (Merkle) Avg Time (Item Count={item_count})")

# Setting plot parameters
plt.xscale("log")  # Logarithmic scale for buffer size
plt.yscale("log")  # Logarithmic scale for time
plt.xlabel("Buffer Size [B]")
plt.ylabel("Average Time [μs]")
# plt.title("MPI Benchmark Results: MPI_Bcast vs MPI_Bcast_Merkle by Item Count")
plt.legend(loc='lower right', fontsize='small')
plt.grid(True, which="both", linestyle="--", linewidth=0.5)
plt.tight_layout()

# Save the plot
plt.savefig("plot.png", dpi=300, bbox_inches='tight')

# Display the plot
plt.show()
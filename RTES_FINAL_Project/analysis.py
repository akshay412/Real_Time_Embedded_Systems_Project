import numpy as np
import matplotlib.pyplot as plt

# Function to convert the txt file to numpy array
def convert_txt_to_array(file_path):
    """
    Reads a text file and converts it into a NumPy array.
    
    Each line in the file should be in the format: gx=..., gy=..., gz=...
    
    Parameters:
        file_path (str): Path to the input text file.
    
    Returns:
        np.ndarray: Array of shape (N, 3) containing gx, gy, gz values.
    """
    with open(file_path, 'r') as file:
        lines = file.readlines()
    
    data = []
    for line in lines:
        values = line.strip().split(", ")
        if len(values) == 3:
            gx, gy, gz = values
            try:
                gx = float(gx.split('=')[1])
                gy = float(gy.split('=')[1])
                gz = float(gz.split('=')[1])
                data.append([gx, gy, gz])
            except (IndexError, ValueError):
                # Handle lines that don't match the expected format
                continue
    
    return np.array(data)

# Normalize signal using min-max normalization (between -1 and 1)
def normalize_signal(data):
    """
    Normalizes the data using min-max normalization to scale values between -1 and 1.
    
    Parameters:
        data (np.ndarray): Array of shape (N, 3).
    
    Returns:
        np.ndarray: Normalized array of the same shape.
    """
    min_vals = np.min(data, axis=0)
    max_vals = np.max(data, axis=0)
    # Avoid division by zero
    denom = max_vals - min_vals
    denom[denom == 0] = 1
    return (data - min_vals) / denom * 2 - 1

# Function to calculate cross-correlation and return maximum correlation
def max_cross_correlation(signal1, signal2):
    """
    Computes the maximum normalized cross-correlation between two signals.
    
    Parameters:
        signal1 (np.ndarray): First signal array.
        signal2 (np.ndarray): Second signal array.
    
    Returns:
        float: Maximum cross-correlation coefficient multiplied by 100.
    """
    correlation = np.correlate(signal1 - np.mean(signal1), signal2 - np.mean(signal2), mode='full')
    normalization = np.std(signal1) * np.std(signal2) * len(signal1)
    if normalization == 0:
        return 0
    max_corr = np.max(correlation) / normalization
    return max_corr * 100  # Convert to percentage

# Function to calculate DTW alignment path with Sakoe-Chiba Band
def dtw_alignment(signal1, signal2, window=None):
    """
    Performs Dynamic Time Warping (DTW) alignment between two signals with an optional Sakoe-Chiba Band constraint.
    
    Parameters:
        signal1 (np.ndarray): First signal array.
        signal2 (np.ndarray): Second signal array.
        window (int, optional): Maximum allowed warping steps. Defaults to None (unconstrained).
    
    Returns:
        tuple: (DTW distance, alignment path list of (i, j) tuples)
    """
    n, m = len(signal1), len(signal2)
    if window is None:
        window = max(n, m)
    window = max(window, abs(n - m))  # Ensure window is at least the difference in lengths
    
    dtw_matrix = np.full((n + 1, m + 1), np.inf)
    dtw_matrix[0, 0] = 0
    
    for i in range(1, n + 1):
        for j in range(max(1, i - window), min(m + 1, i + window + 1)):
            cost = abs(signal1[i - 1] - signal2[j - 1])
            dtw_matrix[i, j] = cost + min(
                dtw_matrix[i - 1, j],    # Insertion
                dtw_matrix[i, j - 1],    # Deletion
                dtw_matrix[i - 1, j - 1] # Match
            )
    
    # Backtrack to find the optimal path
    i, j = n, m
    path = []
    while i > 0 and j > 0:
        path.append((i - 1, j - 1))
        min_prev = min(
            dtw_matrix[i - 1, j],
            dtw_matrix[i, j - 1],
            dtw_matrix[i - 1, j - 1]
        )
        if dtw_matrix[i - 1, j] == min_prev:
            i -= 1
        elif dtw_matrix[i, j - 1] == min_prev:
            j -= 1
        else:
            i -= 1
            j -= 1
    path.reverse()
    return dtw_matrix[-1, -1], path

# Align signals based on DTW path
def align_signals(signal1, signal2, path):
    """
    Aligns two signals based on the DTW path.
    
    Parameters:
        signal1 (np.ndarray): First signal array.
        signal2 (np.ndarray): Second signal array.
        path (list): Alignment path as a list of (i, j) tuples.
    
    Returns:
        tuple: (Aligned signal1, Aligned signal2)
    """
    aligned_signal1 = []
    aligned_signal2 = []
    for i, j in path:
        aligned_signal1.append(signal1[i])
        aligned_signal2.append(signal2[j])
    return np.array(aligned_signal1), np.array(aligned_signal2)

# Function to classify similarity based on weighted mean correlation
def classify_similarity_weighted(results, threshold=92.5):
    """
    Classifies similarity based on weighted mean of post-DTW correlations.
    Weights are assigned based on the correlation values themselves.
    
    Parameters:
        results (dict): Dictionary containing post-DTW correlations for each axis.
        threshold (float): Threshold percentage to classify similarity.
    
    Returns:
        tuple: (Weighted mean correlation, Boolean classification)
    """
    post_dtw_correlations = np.array([results[ax]['post_dtw_correlation'] for ax in results])
    
    # Avoid division by zero
    if np.sum(post_dtw_correlations) == 0:
        return 0, False
    
    weights = post_dtw_correlations / np.sum(post_dtw_correlations)
    weighted_mean = np.sum(weights * post_dtw_correlations)
    
    return weighted_mean, weighted_mean >= threshold

# File paths (Update these paths as per your file locations)
recorded_path = r'C:\Users\bkpou\OneDrive\Desktop\tests\recorded_gesture.txt'
testing_path = r'C:\Users\bkpou\OneDrive\Desktop\tests\testing_data.txt'

# Convert to numpy arrays
recorded = convert_txt_to_array(recorded_path)
testing = convert_txt_to_array(testing_path)



# Check if data was loaded correctly
if recorded.size == 0 or testing.size == 0:
    raise ValueError("One of the input files is empty or incorrectly formatted.")

# Normalize the signals
recorded_normalized = normalize_signal(recorded)
testing_normalized = normalize_signal(testing)

# Calculate cross-correlation, DTW distance, alignment, and post-DTW correlation for each axis
axes = ['gx', 'gy', 'gz']
results = {}

# Define Sakoe-Chiba Band window size (adjust based on your data)
sakoe_chiba_window = int(0.1 * max(len(recorded_normalized), len(testing_normalized)))  # 10% of the longer signal

for i, ax in enumerate(axes):
    # Extract signals for the current axis
    signal_recorded = recorded_normalized[:, i]
    signal_testing = testing_normalized[:, i]
    
    # Calculate maximum cross-correlation before DTW
    pre_dtw_corr = max_cross_correlation(signal_recorded, signal_testing)
    
    # Perform DTW alignment with Sakoe-Chiba Band
    dtw_dist, path = dtw_alignment(signal_recorded, signal_testing, window=sakoe_chiba_window)
    aligned_recorded, aligned_testing = align_signals(signal_recorded, signal_testing, path)
    
    # Calculate Pearson correlation after DTW
    if len(aligned_recorded) < 2:
        post_dtw_corr = 0  # Not enough points to compute correlation
    else:
        post_dtw_corr = np.corrcoef(aligned_recorded, aligned_testing)[0, 1] * 100  # Convert to percentage
    
    results[ax] = {
        'cross_correlation': pre_dtw_corr,
        'dtw_distance': dtw_dist,
        'post_dtw_correlation': post_dtw_corr,
        'aligned_recorded': aligned_recorded,
        'aligned_testing': aligned_testing
    }

# Classification using weighted mean of post-DTW correlations
weighted_mean_correlation, is_similar = classify_similarity_weighted(results)

# Print results
print("Results:")
for ax in axes:
    print(f"\n{ax.upper()}:")
    print(f"Pre-DTW Max Cross-Correlation: {results[ax]['cross_correlation']:.2f}%")
    print(f"DTW Distance: {results[ax]['dtw_distance']:.4f}")
    print(f"Post-DTW Correlation: {results[ax]['post_dtw_correlation']:.2f}%")

print(f"\nWeighted Mean Correlation: {weighted_mean_correlation:.2f}%")
print(f"Classification: {'Similar' if is_similar else 'Not Similar'}")

# Plotting
fig, axs = plt.subplots(3, 2, figsize=(15, 20))
fig.suptitle('Gesture Signal Comparison and Alignment', fontsize=16)

for i, ax_name in enumerate(axes):
    # Plot normalized signals before DTW
    axs[i, 0].plot(recorded_normalized[:, i], label='Recorded')
    axs[i, 0].plot(testing_normalized[:, i], label='Testing')
    axs[i, 0].set_title(f'{ax_name.upper()} - Normalized Signals')
    axs[i, 0].legend()
    axs[i, 0].set_xlabel('Sample Index')
    axs[i, 0].set_ylabel('Normalized Value')
    
    # Plot DTW-aligned signals
    axs[i, 1].plot(results[ax_name]['aligned_recorded'], label='Aligned Recorded')
    axs[i, 1].plot(results[ax_name]['aligned_testing'], label='Aligned Testing')
    axs[i, 1].set_title(f'{ax_name.upper()} - DTW Aligned Signals')
    axs[i, 1].legend()
    axs[i, 1].set_xlabel('Aligned Sample Index')
    axs[i, 1].set_ylabel('Normalized Value')

plt.tight_layout(rect=[0, 0.03, 1, 0.97])  # Adjust layout to make room for the suptitle
plt.show()

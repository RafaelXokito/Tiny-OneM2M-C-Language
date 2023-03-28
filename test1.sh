#!/bin/bash

output_file="curl_times.csv"
count=0
sum=0
min=1000000
max=0
sq_diff_sum=0

# URL to send the requests
url="https://example.com"

# Clean up previous output file, if any
rm -f "$output_file"

# Headers for the CSV file
echo "Request,Time" > "$output_file"

# Perform 500 requests
while [ $count -lt 500 ]; do
    time_ms=$(curl -w "%{time_total}" -o /dev/null -s $url)

    # Store the time in the CSV file
    echo "$count,$time_ms" >> "$output_file"

    # Update the min, max, and sum
    if [ $(echo "$time_ms < $min" | bc) -eq 1 ]; then
        min=$time_ms
    fi
    if [ $(echo "$time_ms > $max" | bc) -eq 1 ]; then
        max=$time_ms
    fi
    sum=$(echo "$sum + $time_ms" | bc)

    count=$((count+1))
done

# Calculate the average
average=$(echo "scale=4; $sum / $count" | bc)

# Calculate the standard deviation
count=0
while read line; do
    if [ $count -gt 0 ]; then
        time_ms=$(echo $line | cut -d',' -f2)
        diff=$(echo "$time_ms - $average" | bc)
        sq_diff=$(echo "$diff * $diff" | bc)
        sq_diff_sum=$(echo "$sq_diff_sum + $sq_diff" | bc)
    fi
    count=$((count+1))
done < "$output_file"

std_dev=$(echo "scale=4; sqrt($sq_diff_sum / ($count - 1))" | bc)

# Print the min, max, average, and standard deviation
echo "Min: $min"
echo "Max: $max"
echo "Average: $average"
echo "Standard Deviation: $std_dev"

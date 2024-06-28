#!/bin/bash

output_file="three_curl_times.csv"
count=0
sum=0
min=1000000
max=0
sq_diff_sum=0

# URL to send the requests
url="http://127.0.0.1:6000/onem2m?fu=1&ty=2&lbl=xpto2&expireAfter=20230426T141939&expireBefore=20230428T105929&filteroperation=AND"

# Clean up previous output file, if any
rm -f "$output_file"

# Headers for the CSV file
echo "Request,Time" > "$output_file"

# Perform 500 requests
while [ $count -lt 500 ]; do
    time_s=$(curl -X GET -s -w "%{time_total}" -o /dev/null "$url")

    # Store the time in the CSV file
    echo "$count,$time_s" >> "$output_file"

    # Update the min, max, and sum
    if [ $(echo "$time_s < $min" | bc) = 1 ]; then
        min=$time_s
    fi
    if [ $(echo "$time_s > $max" | bc) = 1 ]; then
        max=$time_s
    fi
    sum=$(echo "$sum + $time_s" | bc)

    count=$((count+1))
done

# Calculate the average
average=$(echo "scale=8; $sum / $count" | bc)

# Calculate the standard deviation
count=0
while read line; do
    if [ $count -gt 0 ]; then
        time_s=$(echo $line | cut -d',' -f2)
        diff=$(echo "$time_s - $average" | bc)
        sq_diff=$(echo "$diff * $diff" | bc)
        sq_diff_sum=$(echo "$sq_diff_sum + $sq_diff" | bc)
    fi
    count=$((count+1))
done < "$output_file"

std_dev=$(echo "scale=8; sqrt($sq_diff_sum / ($count - 1))" | bc)

# Print the min, max, average, and standard deviation
echo "Min: $min"
echo "Max: $max"
echo "Average: $average"
echo "Standard Deviation: $std_dev"

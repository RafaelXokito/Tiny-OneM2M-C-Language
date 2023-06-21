#!/bin/bash

# Helper function to calculate min, max, avg, and std deviation
calculate_stats() {
    local operation=$1
    local file=$2

    echo "Calculating stats for ${operation}"
    awk -F, '{
        sum+=$1; sumsq+=$1*$1; if (NR==1) {min=max=$1} else {if ($1>max) max=$1; if ($1<min) min=$1}
    } END {
        printf("Min: %.6f\nMax: %.6f\nAvg: %.6f\nStd Dev: %.6f\n", min, max, sum/NR, sqrt(sumsq/NR - (sum/NR)^2))
    }' "$file"
}

# Create CSV files for each operation
post_times="post_times_openmtc.csv"
put_times="put_times_openmtc.csv"
get_times="get_times_openmtc.csv"
delete_times="delete_times_openmtc.csv"

# Remove old CSV files if they exist
rm -f "$post_times" "$put_times" "$get_times" "$delete_times"

count=0

# Perform CRUD operations and store timings in CSV files
while [ $count -lt 200 ]; do
    # POST
    time=$(curl --location --request POST 'http://10.79.12.208:8000/onem2m/lightbulb/state' \
--header 'X-M2M-Origin: admin:admin' \
--header 'Content-Type: application/json;ty=23' \
--data-raw '{
    "m2m:sub": {
        "nu":   ["mqtt://10.79.12.253:1883"],
        "enc":  "DELETE"
    }
}
' -w "%{time_total}" -o /dev/null)
    echo "$time" >> "$post_times"

    count=$((count+1))
done

# count=0

# while [ $count -lt 2001 ]; do
#     # PUT
#     time=$(curl -X PUT "http://127.0.0.1:6000/onem2m/AE-${count}" -H "Content-Type: application/vnd.onem2m-res+json" -d '{"m2m:ae": {"et": "20230331T234737","rr": "true","poa": ["http://127.0.0.1:4314"]}}' -w "%{time_total}" -o /dev/null)
#     echo "$time" >> "$put_times"

#     count=$((count+1))
# done

# count=0

# while [ $count -lt 200 ]; do
#     # GET
#     time=$(curl -X GET "http://10.20.246.135:6000/onem2m?fu=1&ty=2&lbl=xpto2&limit=500" -w "%{time_total}" -o /dev/null)
#     echo "$time" >> "$get_times"

#     count=$((count+1))
# done

# count=0

# while [ $count -lt 200 ]; do
#     # GET
#     time=$(curl -X GET "http://10.20.246.135:10000/onem2m?fu=1&ty=2&lbl=xpto2&limit=500" -w "%{time_total}" -o /dev/null)
#     echo "$time" >> "$get_times"

#     count=$((count+1))
# done

# count=0

# while [ $count -lt 1000 ]; do
#     # DELETE
#     time=$(curl -X DELETE "http://127.0.0.1:6000/onem2m/AE-A${count}" -w "%{time_total}" -o /dev/null)
#     echo "$time" >> "$delete_times"

#     count=$((count+1))
# done

# Calculate and display stats for each operation
calculate_stats "POST" "$post_times"
# calculate_stats "PUT" "$put_times"
# calculate_stats "GET" "$get_times"
# calculate_stats "DELETE" "$delete_times"

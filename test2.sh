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

count=1

# Perform CRUD operations and store timings in CSV files
while [ $count -lt 500 ]; do
    # POST
    time=$(curl -X POST "http://127.0.0.1:6000/onem2m" -H "Content-Type: application/vnd.onem2m-res+json" -d "{\"m2m:ae\": {\"api\": \"placeholder2\",\"rr\": \"true\",\"rn\": \"AE-A${count}\",\"et\": \"20230430T234737\",\"lbl\": [\"interropetores\",\"xpto2\"],\"poa\": [\"http://127.0.0.1:1234\"],\"acpi\": [\"/id-in/acpCreateACPs\"]}}" -w "%{time_total}" -o /dev/null)
    echo "$time" >> "$post_times"

    count=$((count+1))
done

# count=0

# while [ $count -lt 2001 ]; do
#     # PUT
#     time=$(curl -X PUT "http://10.20.246.135:6000/onem2m/AE-${count}" -H "Content-Type: application/vnd.onem2m-res+json" -d '{"m2m:ae": {"et": "20230331T234737","rr": "true","poa": ["http://127.0.0.1:4314"]}}' -w "%{time_total}" -o /dev/null)
#     echo "$time" >> "$put_times"

#     count=$((count+1))
# done

# count=0

# while [ $count -lt 2001 ]; do
#     # GET
#     time=$(curl -X GET "http://127.0.0.1:6000/onem2m/AE-${count}" -w "%{time_total}" -o /dev/null)
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

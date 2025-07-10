#!/bin/bash

# Targets
declare -a targets=("circuitdojo_feather_nrf9160/nrf9160/ns" "circuitdojo_feather_nrf9161/nrf9161/ns")

# Applications
declare -a apps=("accelerometer" "accelerometer_zbus" "active_sleep" "at_client" "battery" "blinky" "bme280" "button" "deep_sleep" "direct_i2c_access" "external_flash" "external_rtc" "external_rtc_time_sync" "gps" "led_pwm" "mfw_update" "sms")

# Combinations to ignore
declare -a ignore_combinations=("circuitdojo_feather_nrf9160/nrf9160/ns:deep_sleep" "circuitdojo_feather_nrf9161/nrf9161/ns:external_rtc" "circuitdojo_feather_nrf9161/nrf9161/ns:external_rtc_time_sync" "circuitdojo_feather_nrf9161/nrf9161/ns:led_pwm")

# Get Git tags
git fetch --prune --tags
version=$(git describe --tags --long)

# Make output dir
mkdir -p .out

# Stop on error
set -e

# Function to check if combination should be ignored
should_ignore() {
    local combination="$1:$2"

    echo Combination: $combination

    for ignore in "${ignore_combinations[@]}"; do
        if [[ "$ignore" == "$combination" ]]; then
            return 0
        fi
    done
    return 1
}

# For each target
for app in "${apps[@]}"
do


# Echo version
echo "Building ${app} (ver: ${version}) for ${target}".

# Change directory
cd samples/${app}

# For each target
for target in "${targets[@]}"; 
do
# Check if this combination should be ignored
if should_ignore "$target" "$app"; then
    echo "Ignoring ${app} for ${target}"
    continue
fi

# Grab part of target before first /
target_basic=$(echo $target | cut -d'/' -f 1)

# Build the target. Continue for loop if error
west build -b $target -d build/$target/ --sysbuild

# Copy the target files over
mkdir -p ../../.out/${version}/${app}

# If app_update.bin exists, copy it
if [ -f build/$target/zephyr/app_update.bin ]; then
    cp build/$target/app_update.bin ../../.out/${version}/${app}/${app}_${target_basic}_${version}_update.bin
fi

# Copy hex
cp build/$target/merged.hex ../../.out/${version}/${app}/${app}_${target_basic}_${version}_merged.hex
done

# Go back
cd ../..

done
import cv2
import tensorflow as tf
import numpy as np
import serial
import time
import tkinter as tk
from PIL import Image, ImageTk
import threading
import requests
import json

# Serial port configuration
serial_port = "COM10"  # Replace with your serial port
baud_rate = 9600

# TensorFlow SSD model loading (replace with your model path)
model_path = (
    "saved_model"  # e.g., 'ssd_mobilenet_v2_fpnlite_320x320_coco17_tpu-8/saved_model'
)
detect_fn = tf.saved_model.load(model_path).signatures["serving_default"]


def load_image_into_numpy_array(image):
    (im_width, im_height) = image.size
    return np.array(image.getdata()).reshape((im_height, im_width, 3)).astype(np.uint8)


def run_inference_for_single_image(model, image):
    image = np.asarray(image)
    input_tensor = tf.convert_to_tensor(image)
    input_tensor = input_tensor[tf.newaxis, ...]
    detections = model(input_tensor)
    num_detections = int(detections.pop("num_detections"))
    detections = {
        key: value[0, :num_detections].numpy() for key, value in detections.items()
    }
    detections["num_detections"] = num_detections
    detections["detection_classes"] = detections["detection_classes"].astype(np.int64)
    return detections


def detect_bottle(image_np, detect_fn, min_score_thresh=0.3):
    detections = run_inference_for_single_image(detect_fn, image_np)
    detected_bottles = False
    for i in range(int(detections["num_detections"])):
        if detections["detection_scores"][i] > min_score_thresh:
            class_id = int(detections["detection_classes"][i])
            if class_id == 44:
                detected_bottles = True
                break
    return detected_bottles


def serial_thread(image_label, result_label, root):
    try:
        ser = serial.Serial(serial_port, baud_rate)
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("Error: Could not open webcam.")
            return

        bottle_count = 0
        scan_mode = False
        image_has_bottle = False  # flag to prevent multiple counts per image

        while True:
            ret, frame = cap.read()
            if not ret:
                print("Error: Could not read frame.")
                continue

            image_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            image_pil = Image.fromarray(image_rgb)
            image_tk = ImageTk.PhotoImage(image_pil)
            image_label.config(image=image_tk)
            image_label.image = image_tk
            root.update()

            if ser.in_waiting > 0:
                line = ser.readline().decode("utf-8").rstrip()
                if line == "scan mode":
                    scan_mode = True
                    bottle_count = 0
                    image_has_bottle = False
                    print("Scan mode started")
                elif line == "end scan":
                    scan_mode = False
                    print("Scan mode ended")
                    if bottle_count > 0:
                        seconds = bottle_count * 5 * 60
                        try:
                            response = requests.get(
                                f"http://192.168.1.18:5000/addvoucher?seconds={seconds}"
                            )
                            response_json = json.loads(response.text)
                            voucher_code = response_json.get("voucherCode")
                            if voucher_code:
                                ser.write(
                                    f"{voucher_code}\n".encode()
                                )  # send in the correct format
                                print(f"Voucher code sent: {voucher_code}")
                            else:
                                print("Error: Voucher code not found in response.")
                        except requests.exceptions.RequestException as e:
                            print(f"Error making request: {e}")

                elif scan_mode and line == "Object detected!":
                    image_np = load_image_into_numpy_array(image_pil)
                    if detect_bottle(image_np, detect_fn) and not image_has_bottle:
                        bottle_count += 1
                        image_has_bottle = True  # set flag to prevent multiple counts
                        print(f"Bottle detected. Count: {bottle_count}")
                        ser.write(
                            "bottle detected\n".encode()
                        )  # send bottle detected message
                    elif not detect_bottle(image_np, detect_fn):
                        image_has_bottle = False  # reset flag if no bottle
                        print(f"no bottle detected")

                        ser.write(
                            "no bottle detected\n".encode()
                        )  # send no bottle detected message.
                    result_label.config(text=f"Bottles: {bottle_count}")

            root.update_idletasks()

        cap.release()
        ser.close()

    except serial.SerialException as e:
        print(f"Serial port error: {e}")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")


def main():
    root = tk.Tk()
    root.title("Bottle Detection")

    camera_label = tk.Label(root)
    camera_label.pack()

    result_label = tk.Label(root, text="")
    result_label.pack()

    serial_thread_instance = threading.Thread(
        target=serial_thread, args=(camera_label, result_label, root)
    )
    serial_thread_instance.daemon = True
    serial_thread_instance.start()

    root.mainloop()


if __name__ == "__main__":
    main()

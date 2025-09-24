import tkinter as tk 
import numpy as np
import scipy as sp
from PIL import Image, ImageTk

# -----------------------------
# GLOBAL VARIABLES
# -----------------------------

# Flag for windows creation
create_flag = False

# FUNCTION DEFINITIONS
# -----------------------------

# create a nuw window
def new_wind(color, name, previous = None):
    """color - name - previous window to hide"""

    if previous and create_flag:

        # Defining the new window with some settings
        new = tk.Toplevel(previous)
        new.geometry("950x650")
        new.title(name)
        new.resizable(False, False)
        new.configure(background=color)

        # Stop showing the previous window if it exists
        previous.withdraw()

        # If closing the new window, destroy also the previous one
        def on_close():
            previous.destroy()
            new.destroy()

        # If closing the main window, destroy also the first one
        new.protocol("WM_DELETE_WINDOW", on_close)

    else: 

        # Defining a new windows without a previous one
        new = tk.Tk()
        new.geometry("950x650")
        new.title(name)
        new.resizable(False, False)
        new.configure(background=color)

    return new

def wind_flag():
    create_flag = True

# Some setting of the graphic interface
wind = new_wind("black", "ESP32 GUI")

# Logo image
image = Image.open("esp32/GUI/j2050logo.png")  
image = image.resize((550, 550))
photo = ImageTk.PhotoImage(image)
canvas = tk.Canvas(wind, width=550, height=550, bg="black",  bd=0, highlightthickness=0)
canvas.place(relx=0.5, rely=0.3, anchor="center")  
canvas.create_image(0, 0, anchor=tk.NW, image=photo)

print(create_flag)
# Button to open a new window
btn_apri = tk.Button(wind, text="START", command=lambda: wind_flag(), bg="white", fg="black", font=("Arial", 16))
btn_apri.place(relx=0.5, rely=0.8, anchor="center")
print(create_flag)
new_E = new_wind("black", "MAIN WINDOW", wind)

# Plotting the graphic interface
wind.mainloop()
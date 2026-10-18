import sys
import os

def select_folder_gui(initial_dir=""):
    try:
        import tkinter as tk
        from tkinter import filedialog, messagebox
    except ImportError:
        return None

    selected = {"path": None}
    
    root = tk.Tk()
    root.title("ZuCompleluxe - Patch Zuma Deluxe")
    root.geometry("540x220")
    root.resizable(False, False)
    
    # Center on screen
    root.update_idletasks()
    x = (root.winfo_screenwidth() - 540) // 2
    y = (root.winfo_screenheight() - 220) // 2
    root.geometry(f"+{x}+{y}")
    
    # Header label
    lbl = tk.Label(root, text="Select Zuma Deluxe installation folder to patch:\n(Folder containing Zuma.exe)", justify="left", font=("Segoe UI", 10))
    lbl.pack(padx=20, pady=(15, 10), anchor="w")
    
    # Path frame
    frame = tk.Frame(root)
    frame.pack(fill="x", padx=20, pady=5)
    
    entry_var = tk.StringVar(value=initial_dir)
    entry = tk.Entry(frame, textvariable=entry_var, font=("Segoe UI", 10))
    entry.pack(side="left", fill="x", expand=True, padx=(0, 10))
    
    def on_browse():
        cur = entry_var.get()
        init = cur if os.path.isdir(cur) else os.getcwd()
        folder = filedialog.askdirectory(initialdir=init, title="Select Zuma Deluxe Folder")
        if folder:
            entry_var.set(os.path.normpath(folder))
            
    btn_browse = tk.Button(frame, text="Browse...", width=10, command=on_browse, font=("Segoe UI", 9))
    btn_browse.pack(side="right")
    
    # Buttons frame
    btn_frame = tk.Frame(root)
    btn_frame.pack(fill="x", padx=20, pady=(25, 15))
    
    def on_patch():
        p = entry_var.get().strip()
        if not p or not os.path.isdir(p):
            messagebox.showerror("Error", "Invalid folder or folder does not exist!")
            return
        zuma_exe = os.path.join(p, "Zuma.exe")
        if not os.path.isfile(zuma_exe):
            res = messagebox.askyesno("Confirmation", "Zuma.exe was not found in this folder.\nAre you sure you want to select this folder?")
            if not res:
                return
        selected["path"] = p
        root.destroy()
        
    def on_cancel():
        root.destroy()
        
    btn_cancel = tk.Button(btn_frame, text="Cancel", width=12, command=on_cancel, font=("Segoe UI", 9))
    btn_cancel.pack(side="right", padx=(10, 0))
    
    btn_patch = tk.Button(btn_frame, text="Patch Folder", width=16, bg="#28a745", fg="white", font=("Segoe UI", 10, "bold"), command=on_patch)
    btn_patch.pack(side="right")
    
    root.mainloop()
    return selected["path"]

if __name__ == '__main__':
    default_dir = sys.argv[1] if len(sys.argv) > 1 else ""
    path = select_folder_gui(default_dir)
    if path:
        print(path)
        sys.exit(0)
    else:
        sys.exit(1)

## conan
smol music recommendation engine based on local library folders.


![alt text](image.png)
*ui is not final*
## run
only tested for windows and WSL.

### foobar2000
install [beefweb from here](https://github.com/hyperblast/beefweb/releases/tag/v0.8). 

then from f2k, files -> settings -> tools -> beefweb remote control -> tick allow remote connections and set port. create `.env` file in root folder and fill it in
```bash
# dont forget the /api at the end
API_URL=http://localhost:8880/api
```
### py
essentia-tensorflow does not support Windows (essentia have support, but we need the classifier models). WSL have auto passthrough for X11, so we can run any GUI applications in it.

optional: setup gpu
```bash
sudo apt install nvidia-cuda-toolkit nvidia-cudnn
sudo ln -s /usr/lib/x86_64-linux-gnu/libcudart.so /usr/local/cuda/lib64/libcudart.so.11.0
sudo ln -s /usr/local/cuda/lib64/libcublas.so.12 /usr/local/cuda/lib64/libcublas.so.11
sudo ln -s /usr/local/cuda/lib64/libcublasLt.so.12 /usr/local/cuda/lib64/libcublasLt.so.11
sudo ln -s /usr/local/cuda/lib64/libcufft.so.11 /usr/local/cuda/lib64/libcufft.so.10
sudo ln -s /usr/local/cuda/lib64/libcusparse.so.12 /usr/local/cuda/lib64/libcusparse.so.11
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

```bash
# skip if you have pyenv
curl https://pyenv.run | bash
pyenv install 3.11.11
pyenv virtualenv 3.11.11 conan
pyenv activate conan
uv pip install -r requirements.txt
# libnss3 and libasound2t64 is required to run browser engine in QT.
sudo apt-get install libnss3 libasound2t64
python main.py
```
if you see any errors, it is %100 related to a missing library in WSL system. read the error, and you will find out what is missing. 

for accessing your Windows folders from the WSL (which, how I use this right now), use the /mnt/ drive in your home folder to access the windows drives
```bash
$ ls /mnt/g/music/Pallbearer/2017\ -\ Heartless/
'01. I Saw the End.mp3'  '03. Lie of Survival.mp3'     '05. Cruel Road.mp3'  '07. A Plea for Understanding.mp3'   folder.jpg
'02. Thorns.mp3'         '04. Dancing in Madness.mp3'  '06. Heartless.mp3'    
```

not tested in linux and macos. 


## todo

- queue / play for similar tracks

- marquee effect for long labels in lists

with long track titles such as 
```
"GOVERNMENT CAME" (9980.0kHz 3617.1kHz 4521.0 kHz) / Cliffs Gaze / cliffs' gaze at empty waters' rise / ASHES TO SEA or NEARER TO THEE
```
labels wraps without preserving the value before being wrapped:

![alt text](./static/t1.png)

and search is broken afterwards:

![alt text](./static/t2.png)

where it works fine if searched by wrapped text:

![alt text](./static/t3.png)

- ???
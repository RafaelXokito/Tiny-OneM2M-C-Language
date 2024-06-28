# 🚀 Tiny-OneM2M C API

![Cool C Language GIF](https://media.giphy.com/media/MT5UUV1d4CXE2A37Dg/giphy.gif)

Welcome to the **Tiny-OneM2M C API** repository! Dive into a world where the C programming language meets the OneM2M IoT platform. This isn't just any library; it's your gateway to seamless M2M communication.

## 🌐 What's Inside?

**Machine-to-machine (M2M)** and **Machine Type Communication (MTC)** are the unsung heroes of the IoT realm. They're the silent whispers allowing devices to chat, share secrets, and collaborate on grand tasks.

Enter **OneM2M** - the global standard that's like the United Nations for devices. It doesn't matter where your device comes from; OneM2M ensures they all speak the same language. And with **Tiny-OneM2M C API**, this conversation just got a lot more interesting.

### 🚗 Why Drive with C?

- 🚀 **Speed Demon**: Direct access to hardware resources? Check. Memory management? Check. C is the Usain Bolt of programming languages.
- 🎯 **Precision**: Perfect for environments where devices run on that one last sip of battery and have memory the size of a goldfish.
- 🤖 **Born for Embedded**: C's middle name is Embedded. It thrives in the nitty-gritty world of embedded systems.

## 📚 Dive into the Docs

Craving the nitty-gritty details? The [documentation directory](https://rafaelxokito.github.io/Tiny-OneM2M-C-Language/?documentation=true) is your treasure trove. From the ABCs of the API to sample spells (code) and the secret sauce to install and wield this library - it's all there.

## 🚀 Launch Sequence

Ready to take off? Strap in and follow these steps:

```bash
# Clone this intergalactic repository
git clone https://github.com/RafaelXokito/Tiny-OneM2M-C-Language.git

# Enter the spaceship's control room
cd TinyOneM2M

# Fire up the engines (Make sure you have Make installed)
make

# Execute the program
./main.o
```

## Tests

Make sure you have the poetry environment by running:

```bash
poetry install
```

Run the tests:
```bash
poetry run pytest --reruns 3 --reruns-delay 1
```

## Roadmap

[] - End2End tests by testing the creation and the usage in a real context.

[] - Test the usage of CNT, CIN and SUB resources, for instance test if the mbs property, and so on change after update.

[] - Create long CRUD tests that check every handled resource attribute.

[] - Check why when you pass ["TAG"] as lbl attribute for the SUB resource the server crash.

[] - Run Valgrind to check memory leaks and fix them. Run the tests to cover as much as possible. 

🌌 Explore. 🚀 Innovate. 🌠 Conquer with Tiny-OneM2M C API.

import OpenEXR
import Imath
import numpy as np


def read_exr(file_path):
    """Reads an EXR file into a numpy array."""
    exr_file = OpenEXR.InputFile(file_path)
    header = exr_file.header()
    dw = header['dataWindow']
    size = (dw.max.x - dw.min.x + 1, dw.max.y - dw.min.y + 1)

    channels = header['channels']
    if 'R' in channels and 'G' in channels and 'B' in channels:
        # Read RGB channels
        red = np.frombuffer(exr_file.channel('R'), dtype=np.float32).reshape(size[1], size[0])
        green = np.frombuffer(exr_file.channel('G'), dtype=np.float32).reshape(size[1], size[0])
        blue = np.frombuffer(exr_file.channel('B'), dtype=np.float32).reshape(size[1], size[0])
        return np.stack((red, green, blue), axis=-1)
    else:
        # Handle single-channel EXR files
        gray = np.frombuffer(exr_file.channel('Y'), dtype=np.float32).reshape(size[1], size[0])
        return gray[..., np.newaxis]


def write_exr(output_path, array):
    """Writes a numpy array to an EXR file."""
    height, width, channels = array.shape
    header = OpenEXR.Header(width, height)
    if channels == 1:
        # Single-channel output
        header['channels'] = {'Y': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT))}
        exr = OpenEXR.OutputFile(output_path, header)
        exr.writePixels({'Y': array.astype(np.float32).tobytes()})
    elif channels == 3:
        # RGB output
        header['channels'] = {
            'R': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT)),
            'G': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT)),
            'B': Imath.Channel(Imath.PixelType(Imath.PixelType.FLOAT)),
        }
        exr = OpenEXR.OutputFile(output_path, header)
        exr.writePixels({
            'R': array[:, :, 0].astype(np.float32).tobytes(),
            'G': array[:, :, 1].astype(np.float32).tobytes(),
            'B': array[:, :, 2].astype(np.float32).tobytes(),
        })
    exr.close()


def horizontal_concat_exr(file, times, output_path):
    """Horizontally concatenates two EXR files."""
    img = read_exr(file)
    result = img.copy()
    # Concatenate images horizontally
    for _ in range(times):
        result = np.concatenate((result, img), axis=1)

    # Write the concatenated result to an EXR file
    write_exr(output_path, result)


# Example usage
file = 'textures/starry_night.exr'
times = 2
output_path = f"starry_night_{times}.exr"
horizontal_concat_exr(file, times, output_path)
print("Concatenation completed. Saved as:", output_path)

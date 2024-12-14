from PIL import Image, ImageDraw, ImageFont
import os


def create_labeled_image_grid(image_folder, row_captions, col_captions, output_path):
    """
    Creates an image grid with row and column captions.

    :param image_folder: Path to the folder containing the images
    :param row_captions: List of row captions
    :param col_captions: List of column captions
    :param output_path: Path to save the output grid image
    """
    # Load a larger font
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", size=40)

    # Load images for each row and column
    image_dict = {}
    for row_caption in row_captions:
        images = []
        for col_index, col_caption in enumerate(col_captions):
            filename = f"sphere_disney-{row_caption}-{col_index:02d}.png"
            filepath = os.path.join(image_folder, filename)
            if os.path.exists(filepath):
                images.append(Image.open(filepath))
            else:
                print(f"Warning: Image not found: {filepath}")
        image_dict[row_caption] = images

    # Determine grid size
    num_rows = len(row_captions)
    num_cols = len(col_captions)
    img_width, img_height = image_dict[row_captions[0]][0].size

    # Set caption dimensions
    caption_height = 100  # Height for column captions
    row_caption_width = 700  # Width for row captions

    # Calculate total grid dimensions
    grid_width = row_caption_width + num_cols * img_width
    grid_height = caption_height + num_rows * img_height

    # Create the output canvas
    grid_img = Image.new('RGB', (grid_width, grid_height), color=(255, 255, 255))
    draw = ImageDraw.Draw(grid_img)

    # Add column captions at the top
    for col, col_caption in enumerate(col_captions):
        x = row_caption_width + col * img_width + img_width // 2
        draw.text((x, caption_height // 2), col_caption, fill="black", anchor="mm", font=font)

    # Add row captions and place images in the grid
    for row, row_caption in enumerate(row_captions):
        y = caption_height + row * img_height + img_height // 2
        draw.text((row_caption_width // 2, y), row_caption, fill="black", anchor="mm", font=font)

        # Place images in the corresponding row
        for col, img in enumerate(image_dict[row_caption]):
            x = row_caption_width + col * img_width
            y = caption_height + row * img_height
            grid_img.paste(img, (x, y))

    # Save the final grid image
    grid_img.save(output_path)
    print(f"Grid creation complete! Image saved at: {output_path}")


# Example: Call the function
if __name__ == "__main__":
    # Set row and column captions
    row_captions = [
        "subsurface", "metallic", "specular", "specularTint",
        "roughness", "anisotropic", "clearcoat", "clearcoatGloss",
        "sheen", "sheenTint"
    ]
    col_captions = ["0.0", "0.1", "0.2", "0.3", "0.4", "0.5", "0.6", "0.7", "0.8", "0.9", "1.0"]

    # Path to the folder containing the images
    image_folder = "./param_exp"  # Replace with the actual path to your image folder
    output_path = "disney.png"

    # Call the function to create the labeled grid
    create_labeled_image_grid(image_folder, row_captions, col_captions, output_path)

import logging

from esphome import core
from esphome.components import display, font, image
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_RAW_DATA_ID,
)
from esphome.core import CORE, HexInt

CONF_DIMENSIONS = "dimensions"

_LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["display"]
MULTI_CONF = True

ImageType = display.display_ns.enum("ImageType")
ColorPicker_ = display.display_ns.class_("Image", image.Image_)

CP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(ColorPicker_),
        cv.Optional(CONF_DIMENSIONS): cv.dimensions,
        cv.GenerateID(CONF_RAW_DATA_ID): cv.declare_id(cg.uint8),
    }
)

CONFIG_SCHEMA = cv.All(font.validate_pillow_installed, CP_SCHEMA)

async def to_code(config):
    from PIL import Image
    import colorsys
    import math

    (width, height) = config[CONF_DIMENSIONS]

    im = Image.new("RGB", (width,height))
    radius = min(im.size)/2.0
    cx, cy = im.size[0]/2, im.size[1]/2
    pix = im.load()

    rhs = []

    for x in range(im.width):
        for y in range(im.height):
            rx = x - cx
            ry = y - cy
            s = (rx ** 2.0 + ry ** 2.0) ** 0.5 / radius
            rgb = (0, 0, 0)
            if s <= 1.0:
                h = ((math.atan2(ry, rx) / math.pi) + 1.0 + math.pi/3.) / 2.0
                rgb = [int(round(c*255.0)) for c in colorsys.hsv_to_rgb(h, s, 1.0)]
                pix[x,y] = tuple(rgb)

    for y in range(im.height):
        for x in range(im.width):
            for component in pix[x,y]:
                rhs.append(component)

    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.new_Pvariable(
        config[CONF_ID], prog_arr, width, height, ImageType.IMAGE_TYPE_RGB24
    )

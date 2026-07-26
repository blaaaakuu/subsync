def boundedInt(value, minimum, maximum):
    """Convert a numeric GUI value to the bounded integer wx controls require."""
    value = int(round(float(value)))
    return max(int(minimum), min(int(maximum), value))


def milliseconds(seconds):
    """Convert seconds to the positive integer milliseconds wx.Timer requires."""
    return max(1, int(round(float(seconds) * 1000)))

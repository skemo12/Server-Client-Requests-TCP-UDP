from pydoc_data.topics import topics
import sys
import signal
import time
import os
import pprint
import json

from contextlib import contextmanager
from subprocess import Popen, PIPE, STDOUT
from os import path
from time import sleep
####### Helper functions #######
@contextmanager
def timeout(time):
  """Raises a TimeoutError after a duration specified in seconds."""
  signal.signal(signal.SIGALRM, raise_timeout)
  signal.alarm(time)

  try:
    yield
  except TimeoutError:
    pass
  finally:
    signal.signal(signal.SIGALRM, signal.SIG_IGN)

def raise_timeout(signum, frame):
  """Raises a TimeoutError."""
  raise TimeoutError

class Topic:
  """Class that represents a subscription topic with data."""

  def __init__(self, name, category, value):
    self.name = name
    self.category = category
    self.value = value

  def print(self):
    """Prints the current topic and data in the expected format."""
    return self.name + " - " + self.category + " - " + self.value

  @staticmethod
  def generate_topics():
    """Generates topics with data for various kinds."""
    ret = []
    ret.append(Topic("a_non_negative_int", "INT", "10"))
    ret.append(Topic("a_negative_int", "INT", "-10"))
    ret.append(Topic("a_larger_value", "INT", "1234567890"))
    ret.append(Topic("a_large_negative_value", "INT", "-1234567890"))
    ret.append(Topic("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwx", "INT", "10"))
    ret.append(Topic("that_is_small_short_real", "SHORT_REAL", "2.30"))
    ret.append(Topic("that_is_big_short_real", "SHORT_REAL", "655.05"))
    ret.append(Topic("that_is_integer_short_real", "SHORT_REAL", "17"))
    ret.append(Topic("float_seventeen", "FLOAT", "17"))
    ret.append(Topic("float_minus_seventeen", "FLOAT", "-17"))
    ret.append(Topic("a_strange_float", "FLOAT", "1234.4321"))
    ret.append(Topic("a_negative_strange_float", "FLOAT", "-1234.4321"))
    ret.append(Topic("a_subunitary_float", "FLOAT", "0.042"))
    ret.append(Topic("a_negative_subunitary_float", "FLOAT", "-0.042"))
    ret.append(Topic("ana_string_announce", "STRING", "Ana are mere"))
    ret.append(Topic("huge_string", "STRING", "abcdefghijklmnopqrstuvwxyz"))
    return ret


class Process:
  """Class that represents a process which can be controlled."""

  def __init__(self, command, cwd=""):
    self.command = command
    self.started = False
    self.cwd = cwd

  def start(self):
    """Starts the process."""
    try:
      if self.cwd == "":
        self.proc = Popen(self.command, universal_newlines=True, stdin=PIPE, stdout=PIPE, stderr=PIPE)
      else:
        self.proc = Popen(self.command, universal_newlines=True, stdin=PIPE, stdout=PIPE, stderr=PIPE, cwd=self.cwd)
      self.started = True
    except FileNotFoundError as e:
      print(e)
      quit()

  def finish(self):
    """Terminates the process and waits for it to finish."""
    if self.started:
      self.proc.terminate()
      self.proc.wait(timeout=1)
      self.started = False

  def send_input(self, proc_in):
    """Sends input and a newline to the process."""
    if self.started:
      self.proc.stdin.write(proc_in + "\n")
      self.proc.stdin.flush()

  def get_output(self):
    """Gets one line of output from the process."""
    if self.started:
      return self.proc.stdout.readline()
    else:
      return ""

  def get_output_timeout(self, tout):
    """Tries to get one line of output from the process with a timeout."""
    if self.started:
      with timeout(tout):
        try:
          return self.proc.stdout.readline()
        except TimeoutError as e:
          return "timeout"
    else:
      return ""

  def get_error(self):
    """Gets one line of stderr from the process."""
    if self.started:
      return self.proc.stderr.readline()
    else:
      return ""

  def get_error_timeout(self, tout):
    """Tries to get one line of stderr from the process with a timeout."""
    if self.started:
      with timeout(tout):
        try:
          return self.proc.stderr.readline()
        except TimeoutError as e:
          return "timeout"
    else:
      return ""

  def is_alive(self):
    """Checks if the process is alive."""
    if self.started:
      return self.proc.poll() is None
    else:
      return False

# default port for the  server
port = "3333"

# default IP for the server
ip = "127.0.0.1"


def run_test_c1_subscribe_all(c1, topics):
  """Tests that subscriber C1 can subscribe to all topics."""
  print("Subscribing C1 to all topics without SF")

  for topic in topics:
    c1.send_input("subscribe " + topic.name + " 0")
    outc1 = c1.get_output_timeout(1)
    if not outc1.startswith("Subscribed to topic."):
      print("Error: C1 not subscribed to all topics")
      failed = True
      break


client = Process(["./subscriber", "C1", ip, port])
client.start()
sleep(1)
topics = Topic.generate_topics()
run_test_c1_subscribe_all(client, topics)
while True:
    outc = client.get_output()
    print(outc)
client.finish()